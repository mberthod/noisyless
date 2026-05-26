# Backlog Autonome NOISYLESS

## Principe

Un cron job (`Backlog_Autonome`, toutes les 15 minutes) lit `~/.hermes-mbhrep/backlog/todo.json`, prend la première tâche `pending`, l'exécute via `delegate_task()`, et poste un tableau de statut dans le salon Discord `#backlog-autonome`.

## Mécanique

```
T+0min   → Cron scanne todo.json → prend la tâche priority la plus basse avec status "pending"
         → Marque "in_progress"
         → Spawn sous-agent (delegate_task) avec toolsets=["terminal", "web"]
         → Exécute le goal avec le context fourni
         → Si succès : marque "done"
         → Si échec : marque "error" + message
         → Poste le tableau complet mis à jour dans #backlog-autonome
```

## Fichier todo.json

Chemin : `~/.hermes-mbhrep/backlog/todo.json`

```json
{
  "version": "2.0",
  "tasks": [
    {
      "id": "identifiant-unique",
      "status": "pending|in_progress|done|error|blocked",
      "priority": 0,  // 0 = critique, plus c'est bas, plus c'est urgent
      "goal": "Ce que le sous-agent doit accomplir (prompt clair et autonome)",
      "context": "Contexte nécessaire (chemins, SSH, dépendances)",
      "tags": ["noisyless", "firmware", "etc"]
    }
  ]
}
```

## Règles

- **UNE seule tâche par run**. Le cron n'exécute jamais plus d'une tâche à la fois.
- **Micro-tâches <5 minutes** : chaque tâche doit être atomique et faisable en <5 min. Découper les grosses.
- **Priorité fixe** : web > API > infra > firmware. **Hardware = Mathieu, jamais dans le backlog**.
- **Documentation obligatoire** : chaque sous-agent log dans `~/.hermes-mbhrep/backlog/logs/YYYY-MM-DD_HHmm_{task_id}.md` (timestamp, actions, fichiers modifiés, résultat).
- **Pas d'envoi client sans T3** → `status: "blocked"`.
- **SSH** : `noisyless@91.99.26.43`, auth clé SSH.
- **Delegate_task** : `context` doit être COMPLET. Le sous-agent ne connaît pas la conversation parente.
- **Timeout/crash** → `status: "error"` + logs.
- **⚠️ Concurrence** : ne JAMAIS écrire `todo.json` manuellement si le cron tourne. Pauser avec `cronjob action=pause job_id=2a926e79d9ba` d'abord.
- **Semi-manuel** → `status: "blocked"` si action humaine requise (ex: clé Stripe).

## Déploiement web

1. `rsync -avz --dry-run web/site/ noisyless@91.99.26.43:/opt/noisyless/static/`
2. `rsync -avz` (sans dry-run)
3. `curl -so /dev/null -w '%{http_code}' http://localhost/` → 200
4. Purge Cloudflare (manuel)

## Déploiement nginx

1. Éditer `Kdrive/01_CLIENTS/NOISYLESS/_infra/nginx/nginx.conf` (pas directement sur le serveur)
2. `scp` le fichier vers le serveur : `scp _infra/nginx/nginx.conf noisyless@91.99.26.43:/opt/noisyless/nginx/nginx.conf`
3. `docker compose -f /opt/noisyless/docker-compose.yml restart nginx`
4. Tester CORS : `curl -sI -X OPTIONS -H 'Origin: https://noisyless.com' -H 'Access-Control-Request-Method: POST' http://localhost/shop/checkout | grep -i access-control`
5. **Ne jamais utiliser `sed`** sur le serveur pour modifier nginx.conf — ça crée des doublons. Toujours partir de la source propre dans Kdrive.

## Déploiement API

1. `rsync` fichier → serveur
2. `docker compose build api` (**pas restart**, ne recharge pas .env)
3. `docker compose up -d api`
4. `docker exec noisyless-api python3 -c "import os; print(os.getenv('KEY')[-4:])"` pour vérifier env vars

## Source

Les tâches viennent de `Kdrive/01_CLIENTS/NOISYLESS/NEWTODO.md` maintenu par Mathieu.

## Ajouter/supprimer des tâches

**En langage naturel** (dans Discord) :
- *"Ajoute une tâche pour réparer le LD6001A"*
- *"Passe fix-mqtt-tls en priorité 1"*
- *"Supprime la tâche document-infra"*
- *"Bloque deploy-api-routers, besoin de valider d'abord"*

**Ou édition directe** : modifier `~/.hermes-mbhrep/backlog/todo.json` → le cron le captera au prochain run.

## Jobs cron associés

| Job | Fréquence | Livraison |
|-----|-----------|-----------|
| `Backlog_Autonome` | Toutes les 15 min | `discord:1508087088946352199` (#backlog-autonome) |
| `EditorBot` | Tous les 2j, 6h | local |
| `Suivi_Projets` | Tous les jours, 8h | local |

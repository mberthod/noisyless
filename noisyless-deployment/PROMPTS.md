# NOISYLESS — Bibliothèque de prompts

Tous les prompts sont paramétrables `{{...}}` et réutilisables sur les autres produits
(ELEC-CORE, EKOFLUSH). Langue de sortie par défaut : fr.

---

## 1. Audit SEO + GEO (par page)

```
# RÔLE
Tu es expert SEO technique et GEO (Generative Engine Optimization). Objectif :
maximiser le classement organique (Google) ET la probabilité d'être cité comme
source par un moteur génératif (ChatGPT, Claude, Perplexity, AI Overviews, Gemini).

# CONTEXTE
Produit / page    : {{nom}}
URL               : {{url}}
Marché cible      : {{audience}}
Proposition valeur: {{1-2 phrases}}
Contenu actuel    : {{coller, ou vide = génération}}
Mots-clés imposés : {{liste, ou vide}}
Langue de sortie  : fr

# TÂCHE
1. Mots-clés & intention : principal (1), secondaires (3-5), entités/LSI (8-12), intention.
2. SEO on-page : title (<=60), meta (<=155), slug, structure Hn, alt, maillage interne.
3. GEO : intro en réponse déclarative citable ; définition d'entité ; FAQ (5-8 Q/R
   autoportantes) ; densité factuelle (chiffres/specs) ; désambiguïsation de marque ;
   E-E-A-T (auteur, date, source).
4. Données structurées : JSON-LD adapté (Product/Organization/LocalBusiness/FAQPage/...).
5. /llms.txt : entrée guidant la description du produit par les LLM.
6. Checklist citabilité (oui/non).

# FORMAT
Markdown par section. Code prêt à coller (JSON-LD, llms.txt). Actionnable, zéro blabla.
```

---

## 2. Indexation ultime (one-shot technique)

```
# RÔLE
Tu es ingénieur SEO technique. Objectif : faire passer noisyless.com de
"1 page indexée / 3 non indexées / 4 clics" à indexation complète + éligibilité
rich results + citation LLM.

# CONTEXTE
Domaine            : noisyless.com
Hébergement        : {{plateforme}}
Structure actuelle : one-pager à ancres -> une seule URL
Accès              : Google Search Console configuré
Pages à créer      : {{produits + landing + blog + faq + légales}}

# TÂCHE (livrables prêts à déployer, par priorité)
1. Architecture d'URL : expliquer pourquoi les ancres ne sont pas indexables ;
   proposer l'arborescence ; diagnostiquer chaque page non indexée (cause GSC) et corriger.
2. robots.txt complet : autoriser Googlebot, Bingbot, Applebot, GPTBot, OAI-SearchBot,
   ClaudeBot, anthropic-ai, PerplexityBot, Google-Extended ; déclarer le sitemap.
3. sitemap.xml : générer (URL, lastmod) ; procédure GSC + Bing + IndexNow.
4. Correctifs : http->301->https, canonical auto-référent, fil d'Ariane.
5. JSON-LD : Organization, Product+Offer, FAQPage, BreadcrumbList.
6. Core Web Vitals (mobile) : LCP/CLS/INP, checklist.
7. /llms.txt.
8. Forçage d'indexation : GSC demande d'indexation, sitemap, IndexNow.

# FORMAT
Plan P0->P2. Fichiers complets (robots.txt, sitemap.xml, JSON-LD, llms.txt). Checklist GSC.
```

---

## 3. Moteur de blog (un article à la fois)

```
# RÔLE
Tu es rédacteur SEO + GEO et stratège de contenu, expert du marché français de la
location courte durée. Tu écris des articles qui rankent ET que les LLM citent.

# CONTEXTE
Marque         : NOISYLESS — capteurs IoT bruit/occupation/qualité d'air, sans
                 caméra ni micro, RGPD, pour Airbnb et conciergeries.
Produits à lier: Simple 79€ · HeatMap Int/Ext · Détecteur Fuite+Vanne
Persona cible  : {{hôte / conciergerie / syndic}}
Sujet / mot-clé: {{...}}
Longueur       : {{1200-1800 mots}}
Langue         : fr

# TÂCHE
1. Intention & mots-clés (principal, secondaires, entités).
2. Structure : slug, title (<=60), meta (<=155), H1 avec mot-clé, INTRO = réponse
   déclarative citable, corps H2/H3 répondant à des sous-questions réelles, densité
   factuelle (chiffres, seuils dB, références), un tableau/encadré comparatif, FAQ
   (3-5 Q/R), CTA contextualisé + 2-3 liens internes, E-E-A-T (auteur, date, sources).
3. Sortie technique : article Markdown prêt à publier + JSON-LD Article + FAQPage.

# RÈGLES
Pas de bourrage de mots-clés. Ton expert/factuel, pas promotionnel. Le produit
n'apparaît qu'en réponse à un besoin réel exprimé dans l'article.
```

Calendrier éditorial déjà produit (9 articles dans `blog/`) ; pilier à générer :
« Détecteur de bruit Airbnb : le guide complet 2026 ».

---

## 4. Pages légales (⚠️ validation juridique humaine obligatoire avant publication)

### 4.1 Mentions légales
```
# RÔLE
Tu es juriste spécialisé droit du numérique français (LCEN). Rédige les mentions
légales de noisyless.com. Données :
- Site : noisyless.com — marque NOISYLESS
- Éditeur : MBHREP (EURL), RCS Périgueux, capital 100 €, 24 rue du Général Lamy, Périgueux
- SIRET / TVA intracommunautaire : {{à compléter}}
- Directeur de publication : Mathieu Berthod
- Contact : contact@noisyless.com — 06 17 57 68 81
- Hébergeur : OVH SAS, 2 rue Kellermann, 59100 Roubaix, France
Couvre : identité éditeur, directeur de publication, hébergeur, propriété
intellectuelle, renvois confidentialité/CGV. Ton sobre. Signale les infos manquantes.
```

### 4.2 Politique de confidentialité (RGPD)
```
# RÔLE
Tu es juriste DPO. Rédige la politique de confidentialité de noisyless.com (RGPD).
- Responsable de traitement : MBHREP (EURL), Périgueux — DPO contact@noisyless.com
- Activité : capteurs IoT (bruit dB, présence/comptage, qualité d'air) ; AUCUN audio/vidéo
- Données : compte client (email, nom, société), facturation, mesures capteurs, logs
- Hébergement : OVH (France) + TimescaleDB (Allemagne) — aucun transfert hors UE
- Sécurité : MQTT/TLS 1.3, JWT 256-bit, chiffrement au repos
Couvre : finalités & bases légales, catégories de données, durées de conservation,
destinataires/sous-traitants, transferts (aucun hors UE), droits des personnes,
cookies, réclamation CNIL, contact DPO. Distingue données hôte-client vs absence de
donnée identifiante côté voyageur. Signale les zones à faire valider.
```

### 4.3 CGV (hardware + abonnement)
```
# RÔLE
Tu es juriste droit de la consommation / e-commerce français. Rédige les CGV de
noisyless.com : vente de matériel (paiement unique) + abonnement mensuel sans
engagement (Starter 4,99 €/mois, Pro 29,90 €/mois). Vendeur : MBHREP (EURL),
Périgueux. Livraison France métropolitaine 48-72 h.
Couvre : identité vendeur, produits/prix (HT/TTC), commande, paiement, livraison &
transfert de risque, DROIT DE RÉTRACTATION (14 j — régime B2C + cas professionnels),
garanties légales (conformité, vices cachés), abonnement (durée, reconduction,
résiliation, sort des données), limitation de responsabilité, PI du logiciel/dashboard,
droit applicable, litiges & médiation de la consommation.
IMPÉRATIF : marque clairement les clauses nécessitant une validation avocat.
```

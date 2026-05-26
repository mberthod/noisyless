# MQTT Ingest Service — Pattern asyncio thread-safe

## Problème

Le callback `on_message` de paho-mqtt-client s'exécute dans un thread séparé, pas dans l'event loop asyncio. Appeler `asyncio.create_task()` directement lève :

```
RuntimeError: no running event loop
```

## Solution — Queue thread-safe

Utiliser une `asyncio.Queue` comme pont entre le thread MQTT et l'event loop principal.

### Pattern complet

```python
import asyncio
import paho.mqtt.client as mqtt

message_queue: asyncio.Queue = None


def on_message(client, userdata, msg):
    """Callback exécuté dans un thread séparé"""
    print(f"Message received: {msg.topic}")
    # Thread-safe : put_nowait ne bloque pas
    if message_queue:
        message_queue.put_nowait((msg.topic, msg.payload))


async def message_processor():
    """Tourne dans l'event loop principal"""
    while True:
        topic, payload = await message_queue.get()
        await process_message(topic, payload)
        message_queue.task_done()


async def process_message(topic: str, payload: bytes):
    """Traitement asyncio-safe"""
    data = json.loads(payload.decode("utf-8"))
    # ... DB insert, etc.


async def main():
    global message_queue
    message_queue = asyncio.Queue()
    
    # Démarrer le processeur dans l'event loop
    asyncio.create_task(message_processor())
    
    # Connexion MQTT
    client = mqtt.Client()
    client.on_message = on_message
    client.connect("localhost", 1883, 60)
    client.loop_start()
    
    # Garder le service en vie
    while True:
        await asyncio.sleep(60)
```

### Pourquoi ça marche

1. **`put_nowait()`** : thread-safe, ne bloque jamais, lève `Full` si queue pleine (utiliser `maxsize` pour limiter)
2. **`await message_queue.get()`** : suspend la tâche jusqu'à ce qu'un message arrive
3. **`task_done()`** : marque le traitement comme terminé (pour `join()` si besoin)

### Alternative : `call_soon_threadsafe`

Moins élégant mais fonctionne :

```python
def on_message(client, userdata, msg):
    loop = asyncio.get_event_loop()
    if loop and loop.is_running():
        loop.call_soon_threadsafe(
            lambda: asyncio.create_task(process_message(msg.topic, msg.payload))
        )
```

**Inconvénient** : ne gère pas les erreurs dans `process_message`, pas de backpressure.

---

## Test de validation

```python
# Dans le conteneur mqtt-ingest
docker exec noisyless-mqtt-ingest python3 -c "
import paho.mqtt.client as mqtt
import json
client = mqtt.Client()
client.connect('mosquitto', 1883, 60)
client.publish('noisyless/NL-001/heatmap', json.dumps({
    'villa_id': 'test',
    'zone': 'salon',
    'total_count': 5,
    'max_count': 3
}))
client.disconnect()
print('✅ Published')
"

# Vérifier les logs
docker logs noisyless-mqtt-ingest 2>&1 | grep -E 'Message received|Inserted'

# Vérifier la DB
docker exec noisyless-timescaledb psql -U noisyless -d noisyless -c \
  "SELECT villa_id, zone, total_count FROM heatmap_agg_5min ORDER BY window_start DESC LIMIT 1;"
```

---

## Fichier de référence

Ce pattern est implémenté dans :
- `/home/mathieu/Kdrive/01_CLIENTS/NOISYLESS/_infra/services/mqtt-ingest/main.py`

Déploiement :
```bash
rsync -av _infra/services/mqtt-ingest/main.py noisyless@91.99.26.43:/opt/noisyless/services/mqtt-ingest/
ssh noisyless@91.99.26.43 "cd /opt/noisyless && docker compose build mqtt-ingest && docker compose up -d mqtt-ingest"
```

---

## Erreurs à éviter

| Erreur | Symptôme | Fix |
|--------|----------|-----|
| `asyncio.create_task()` dans callback | `RuntimeError: no running event loop` | Queue thread-safe |
| `loop.call_soon_threadsafe(asyncio.create_task(...))` | Marche mais erreurs silencieuses | Queue + `task_done()` |
| `docker compose restart` après modif code | Ancienne version tourne | `docker compose build && up -d` |
| INSERT avec mauvais schéma | `column does not exist` | `\d table_name` avant d'insérer |

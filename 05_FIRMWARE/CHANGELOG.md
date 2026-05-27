# Firmware Changelog

## v1.0.8 — 2026-05-27
- 🔴 LED rouge si WiFi perdu dans loop()
- Comportement LED inchangé quand WiFi ok

## v1.0.7 — 2026-05-27
- Swap mapping LED R/G (GPIO12=rouge, GPIO13=vert) après constat inversion physique
- Correction bug LED jaune (vert allumé avant publication MQTT)
- Clignotement bleu version au boot (3ème chiffre)

## v1.0.5 — 2026-05-27
- LEDs en actif bas (LOW = allumé)
- OTA code prêt, sans partitions OTA dédiées

## v1.0.4 — 2026-05-27
- WiFi + MQTT + OTA + capteurs analogiques
- Build ESP32-S3 QIO natif, full erase
- Correction mot de passe WiFi

## v1.0.1 — 2026-05-26
- OTA check toutes les 60s
- WiFi dur, MQTT, capteurs analogiques

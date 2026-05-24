---
title: "RGPD et capteurs en location : guide de conformité pour les hôtes"
description: "Installer des capteurs en location courte durée est conforme au RGPD si l'on respecte transparence, minimisation et absence d'enregistrement audio/vidéo. Guide pratique pour les hôtes."
slug: rgpd-capteurs-location-conformite
date: 2026-05-24
author: L'équipe NOISYLESS
keywords: [rgpd capteur location, conformité airbnb rgpd, capteur bruit données personnelles, cnil location courte durée]
---

# RGPD et capteurs en location : guide de conformité pour les hôtes

**Installer des capteurs dans une location courte durée est conforme au RGPD à condition de respecter trois principes : la transparence (informer le voyageur), la minimisation (ne collecter que le strict nécessaire) et l'absence de tout enregistrement audio ou vidéo. Un capteur qui mesure un niveau de bruit, une présence ou la qualité d'air sans micro ni caméra reste dans le cadre.**

Le RGPD n'interdit pas le monitoring : il encadre la collecte de données personnelles. Tout l'enjeu est de ne pas en collecter inutilement.

## Les trois principes à respecter

- **Transparence et information.** Le voyageur doit être informé de la présence des capteurs, de leur finalité (prévenir les nuisances et les sinistres) et de la nature des données. À indiquer dans l'annonce et le règlement intérieur.
- **Minimisation des données.** On ne collecte que l'utile : niveau sonore en dB, présence binaire ou comptage, mesures environnementales. Pas de contenu audio, pas d'image.
- **Finalité légitime.** La prévention des troubles et la protection du bien sont des finalités recevables ; la surveillance des personnes ne l'est pas.

## Ce qui est interdit

- **Toute caméra** à l'intérieur du logement.
- **Tout micro d'enregistrement** captant le contenu sonore.
- La collecte de données qui permettraient d'**identifier** ou de suivre individuellement un occupant.

## Ce qui est conforme

- Un **niveau sonore en dB**, sans audio : ce n'est pas une donnée identifiante.
- Une **présence ou un comptage** par PIR, ToF ou radar : un nombre, pas une identité.
- Des **mesures environnementales** (CO₂, température, humidité).

## Bonnes pratiques complémentaires

- Choisir un prestataire dont les **données sont hébergées dans l'UE**, avec chiffrement en transit et au repos.
- Vérifier qu'un **DPO** (délégué à la protection des données) est désigné.
- Définir une **durée de conservation** proportionnée des historiques.

> Cet article est informatif et ne constitue pas un avis juridique. En cas de doute, consultez la CNIL ou un professionnel.

## Avec NOISYLESS

Les capteurs [NOISYLESS](/produits/noisyless-simple) ne captent ni image ni son (uniquement dB, présence et qualité d'air), avec données hébergées en Europe (OVH France, base en Allemagne), chiffrement MQTT over TLS 1.3 et DPO désigné — une conformité pensée dès la conception.

## FAQ

**Un niveau sonore en dB est-il une donnée personnelle ?**
Non, s'il n'est pas associé à un enregistrement audio ou à un élément identifiant : c'est une mesure d'intensité, pas une donnée permettant de reconnaître quelqu'un.

**Dois-je faire une déclaration à la CNIL ?**
La logique RGPD repose surtout sur la responsabilisation (registre, information, minimisation) plutôt que sur une déclaration systématique. Référez-vous aux recommandations de la CNIL.

**Où doivent être stockées les données ?**
De préférence dans l'Union européenne, sans transfert hors UE, avec chiffrement.

---

*Voir aussi : [Un détecteur de bruit est-il légal ?](/blog/detecteur-bruit-airbnb-legal) · [Suroccupation : la repérer légalement](/blog/suroccupation-location-courte-duree)*

<!-- JSON-LD -->
<script type="application/ld+json">
{
  "@context": "https://schema.org",
  "@type": "FAQPage",
  "mainEntity": [
    { "@type": "Question", "name": "Un niveau sonore en dB est-il une donnée personnelle ?", "acceptedAnswer": { "@type": "Answer", "text": "Non, s'il n'est pas associé à un enregistrement audio ou à un élément identifiant : c'est une mesure d'intensité, pas une donnée identifiante." } },
    { "@type": "Question", "name": "Où doivent être stockées les données ?", "acceptedAnswer": { "@type": "Answer", "text": "De préférence dans l'Union européenne, sans transfert hors UE, avec chiffrement." } }
  ]
}
</script>

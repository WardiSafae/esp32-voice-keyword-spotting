# 🎙️ ESP32 Voice Keyword Spotting (ON/OFF) — Edge AI 100% Offline

Système embarqué de reconnaissance vocale de mots-clés fonctionnant **entièrement en local sur microcontrôleur**, sans cloud, sans connexion Internet requise pour l'inférence. Le système écoute en continu via un microphone numérique I2S, détecte les commandes "ON" / "OFF" en ~1 seconde, pilote une sortie physique, et publie l'état sur un dashboard IoT via MQTT.

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/ML-Edge%20Impulse-orange)
![Language](https://img.shields.io/badge/language-C%2B%2B-red)
![License](https://img.shields.io/badge/license-MIT-green)

---

## 🎬 Démo

> _Ajouter ici un GIF ou un lien vers `video_demonstration.mp4` (ou une vidéo YouTube non listée) montrant : "je dis ON → la LED s'allume"._

---

## 💡 Pourquoi ce projet

Les assistants vocaux grand public (Alexa, Google Home) reposent sur le cloud : chaque commande vocale part sur un serveur distant, ce qui pose des problèmes de **confidentialité**, de **latence** et de **coût**. Ce projet explore l'alternative **TinyML / Edge AI** : faire tourner un modèle de reconnaissance vocale directement sur un microcontrôleur à moins de 10€, avec une empreinte mémoire compatible avec du matériel très contraint.

## ⚙️ Comment ça marche

```
Microphone I2S (INMP441)
        │  16 kHz, mono, 32-bit
        ▼
 Buffer circulaire (1s = 16000 échantillons)
        │  normalisation int16 → float
        ▼
 Extraction MFCC (13 coefficients)
        │
        ▼
 Modèle CNN 1D quantisé (Edge Impulse / TensorFlow Lite Micro)
        │  classes : "on" / "off" / "noise"
        ▼
 Décision (seuil de confiance 70%)
        │
        ├──► LED (action locale, GPIO)
        └──► Publication MQTT (ThingSpeak) — état + score de confiance
```

Le modèle est entraîné et exporté depuis [Edge Impulse](https://edgeimpulse.com/) sous forme de bibliothèque C++ (`ON_OFF_Keyword_Spotting_inferencing.h`), puis intégré au firmware Arduino qui gère l'acquisition audio (I2S), l'inférence, le pilotage de la LED et la connectivité WiFi/MQTT.

## 🔧 Matériel

| Composant | Rôle | Coût approx. |
|---|---|---|
| ESP32 DevKit v1 | Microcontrôleur, inférence ML | ~6-8€ |
| INMP441 | Microphone numérique I2S | ~3-5€ |
| LED + résistance 220Ω | Indicateur de sortie | ~1€ |

**Coût total : < 20€**

### Câblage

| ESP32 | INMP441 |
|---|---|
| 3.3V | VDD |
| GND | GND |
| GPIO21 | SD (data) |
| GPIO22 | WS (word select) |
| GPIO26 | SCK (clock) |

## 📊 Résultats

**Performance du modèle (jeu de test Edge Impulse) :**

| Classe | Recall |
|---|---|
| ON | 98% |
| OFF | 97% |
| Bruit/silence | 95% |

**Empreinte sur ESP32 :**

| Ressource | Occupation |
|---|---|
| Flash (code + modèle) | ~30% |
| RAM | ~17% |

**Temps réel :** latence acquisition + inférence d'environ **1 seconde**, fonctionnement 100% local pour la détection (le WiFi/MQTT ne sert qu'au reporting optionnel, pas à l'inférence).

## 🛠️ Stack technique

- **Edge AI / TinyML** : Edge Impulse Studio, TensorFlow Lite Micro, extraction MFCC
- **Embarqué** : ESP32 (Arduino framework), driver I2S pour l'audio numérique temps réel
- **Connectivité** : WiFi, MQTT (PubSubClient) vers ThingSpeak pour le monitoring à distance

## 🚀 Reproduire le projet

1. Câbler l'INMP441 et une LED sur l'ESP32 selon le schéma ci-dessus.
2. Créer un projet [Edge Impulse](https://edgeimpulse.com/), collecter des échantillons audio "on" / "off" / "noise" (1s, 16kHz), entraîner un modèle de classification audio (MFCC + CNN 1D), puis l'exporter en **bibliothèque Arduino**.
3. Copier la bibliothèque générée dans `firmware/` et l'inclure dans `Keyword_OnOff.ino`.
4. Copier `config.example.h` vers `config.h` et renseigner tes propres identifiants WiFi / MQTT (voir ci-dessous).
5. Flasher via Arduino IDE (carte ESP32 DevKit v1, baud 115200).
6. Ouvrir le moniteur série et parler dans le micro.

### ⚠️ Configuration des identifiants

Les identifiants WiFi et MQTT **ne sont pas inclus dans ce dépôt**. Créer un fichier `firmware/config.h` (ignoré par Git, voir `.gitignore`) sur le modèle de `config.example.h` :

```cpp
#define WIFI_SSID     "TON_SSID"
#define WIFI_PASSWORD "TON_MOT_DE_PASSE"
#define MQTT_CLIENT_ID "..."
#define MQTT_USERNAME  "..."
#define MQTT_PASSWORD  "..."
```

## 🔍 Limites connues

- Jeu de données "noise" limité, ce qui peut générer des faux positifs en environnement bruyant.
- Seulement 2 commandes reconnues actuellement.
- Feedback utilisateur limité à une LED (pas d'écran).

## 🗺️ Pistes d'amélioration

- Enrichir le dataset de bruit ambiant pour réduire les faux positifs
- Ajouter un écran LCD pour un retour utilisateur plus riche
- Étendre le vocabulaire de commandes reconnues
- Ajouter un mode veille pour l'autonomie sur batterie

## 📁 Structure du dépôt

```
esp32-voice-keyword-spotting/
├── .gitignore
├── README.md
├── LICENSE
├── firmware/
│   ├── Keyword_OnOff.ino
│   ├── config.example.h
│   └── config.h          # (NON COMMITÉ - fichier local uniquement)
├── docs/
│   ├── Presentation_reconnaissance_ON_OFF.pdf
├── media/
│   └── video_demonstration.mp4
└── links.txt
```

## 👤 Auteur

**WARDI Safae** — Master BDSI, Faculté des Sciences Dhar El Mahraz, USMBA Fès
Réalisé en binôme avec Imane Boujaj.

#include <ON_OFF_Keyword_Spotting_inferencing.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/i2s.h>                         // Bibliothèque I2S (Inter-IC Sound) pour l'audio numérique
#include "config.h"


/*
 * Reconnaissance vocale ON/OFF avec ESP32 + INMP441
 * Edge Impulse - MFCC 13 coefficients - Window 1000ms
 * Avec envoi MQTT vers ThingSpeak
 */


// ========== BROCHES ==========
#define I2S_BCK  26   // BCLK - Horloge série qui rythme les données
#define I2S_WS   22   // LRCLK - Sélection mot Indique gauche/droite
#define I2S_DIN  21   // DOUT - Données audio
#define LED_PIN  13   // LED de sortie

// ========== PARAMÈTRES AUDIO ==========
#define SAMPLE_RATE     16000       // 16 000 échantillons par seconde
#define WINDOW_MS       1000        // 1 seconde
#define SAMPLES_PER_WINDOW (SAMPLE_RATE * WINDOW_MS / 1000)  // = 16000

// ========== PARAMÈTRES WiFi ==========
const char* WIFI_SSID = WIFI_SSID;           
const char* WIFI_PASSWORD = WIFI_PASSWORD; 

// ========== PARAMÈTRES MQTT THINGSPEAK ==========
const char* MQTT_BROKER = MQTT_BROKER;       //Serveur MQTT
const int MQTT_PORT = MQTT_PORT;
const char* MQTT_CLIENT_ID = MQTT_CLIENT_ID;     
const char* MQTT_USERNAME = MQTT_USERNAME;       
const char* MQTT_PASSWORD = MQTT_PASSWORD;  
const char* MQTT_TOPIC = MQTT_TOPIC;    

// ========== CLIENTS WIFI ET MQTT ==========
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ========== CONFIGURATION I2S ==========                      I2S est un protocole standard pour l'audio numérique.
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,                         // 16 000 Hz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,       // 32 bits par échantillon
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,        // Un seul canal (mono)
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,           
    .dma_buf_count = 8,                                 // 8 tampons DMA          permettent de lire l'audio sans bloquer le processeur.
    .dma_buf_len = 1024,                                // 1 024 échantillons par tampon
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DIN
};

// ========== BUFFER AUDIO ==========                               une mémoire tampon qui stocke 1 seconde d'audio.
int16_t audio_buffer[SAMPLES_PER_WINDOW];       // Tableau de 16 000 int16 : chaque échantillon est un nombre entier signé de 16 bits
int buffer_index = 0;

// ========== VARIABLES ==========
unsigned long last_inference_time = 0;
const unsigned long INFERENCE_INTERVAL_MS = 1000;  // 1 seconde entre inférences

// Dernières valeurs prédites pour éviter d'envoyer en boucle
String last_prediction = "";
float last_confidence = 0.0;
unsigned long last_publish_time = 0;
const unsigned long PUBLISH_INTERVAL_MS = 3000;  // Envoyer toutes les 3 secondes max

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);                                           // Communication avec le PC
    delay(1000);
    
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║   RECONNAISSANCE VOCALE ON/OFF - ESP32    ║");
    Serial.println("║   Edge Impulse - 13 MFCC - 1000ms window  ║");
    Serial.println("╠═══════════════════════════════════════════╣");
    Serial.println("║   Dites 'ON'  → LED allumée              ║");
    Serial.println("║   Dites 'OFF' → LED éteinte               ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    Serial.println();
    
    // Configuration LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Connexion WiFi
    connectWiFi();
    
    // Configuration MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    connectMQTT();
    
    // Initialisation I2S (microphone)
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);         // Démarrage du microphone
    if (err != ESP_OK) {
        Serial.println("❌ Erreur installation I2S!");
        while(1);
    }
    
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.println("❌ Erreur configuration broches I2S!");
        while(1);
    }
    
    err = i2s_start(I2S_NUM_0);
    if (err != ESP_OK) {
        Serial.println("❌ Erreur démarrage I2S!");
        while(1);
    }
    
    Serial.println("✅ Microphone initialisé avec succès");
    Serial.println("✅ Modèle Edge Impulse chargé");
    Serial.println();
    Serial.println("🎤 En écoute... Parlez maintenant !");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}

// ========== FONCTIONS WIFI ET MQTT ==========
void connectWiFi() {
    Serial.print("📶 Connexion au Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" ✅ Connecté !");
        Serial.print("   Adresse IP : ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(" ❌ Échec de connexion WiFi !");
        Serial.println("   Le projet fonctionnera sans envoi de données.");
    }
}

void connectMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    Serial.print("📡 Connexion à MQTT ThingSpeak...");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println(" ✅ Connecté !");
    } else {
        Serial.print(" ❌ Échec, code erreur : ");
        Serial.println(mqttClient.state());
    }
}

void reconnectMQTT() {
    // Vérifier si le WiFi est toujours connecté
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }
    
    // Tenter de reconnecter MQTT
    if (!mqttClient.connected()) {
        connectMQTT();
    }
}

void publishToThingSpeak(String command, float confidence) {
    // Vérifier la connexion MQTT
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    
    if (!mqttClient.connected()) {
        Serial.println("❌ MQTT non connecté, données non envoyées");
        return;
    }
    
    // Construire le payload au format field1=...&field2=...
    String payload = "field1=";
    
    // field1 : 1 pour ON, 0 pour OFF
    if (command == "ON") {
        payload += "1";
    } else if (command == "OFF") {
        payload += "0";
    } else {
        payload += "2";  // 2 pour "aucune commande claire"
    }
    
    // field2 : score de confiance
    payload += "&field2=";
    payload += String(confidence, 3);  // 3 décimales
    
    // Publier
    if (mqttClient.publish(MQTT_TOPIC, payload.c_str())) {
        Serial.print("  📤 Envoyé à ThingSpeak : ");
        Serial.println(payload);
    } else {
        Serial.println("  ❌ Erreur d'envoi MQTT");
    }
}

// ========== LOOP PRINCIPAL ==========
void loop() {
    // Lecture continue du microphone
    read_audio_continuous(); 
    
    // Inférence toutes les secondes (quand le buffer est plein)
    if (millis() - last_inference_time >= INFERENCE_INTERVAL_MS && buffer_index >= SAMPLES_PER_WINDOW) {
        last_inference_time = millis();
        
        // Exécuter la classification
        run_inference();
        
        // Réinitialiser le buffer pour la prochaine fenêtre
        buffer_index = 0;
    }
    
    // Maintenance MQTT (garder la connexion active)
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();
}

// ========== LECTURE AUDIO EN CONTINU ==========           lit un échantillon I2S, le convertit de 32 bits à 16 bits, et le stocke dans le buffer
void read_audio_continuous() {
    int32_t raw_sample;
    size_t bytes_read;
    
    while (buffer_index < SAMPLES_PER_WINDOW) {
        // Lecture d'un échantillon I2S
        i2s_read(I2S_NUM_0, &raw_sample, sizeof(raw_sample), &bytes_read, portMAX_DELAY);
        
        if (bytes_read > 0) {
            // Conversion 32-bit en 16-bit
            int16_t sample = (int16_t)(raw_sample >> 16);
            audio_buffer[buffer_index] = sample;
            buffer_index++;
        }
    }
}

// ========== INFÉRENCE EDGE IMPULSE ==========
void run_inference() {
    // Créer la structure signal pour Edge Impulse
    signal_t signal;
    signal.total_length = SAMPLES_PER_WINDOW;
    signal.get_data = &get_audio_data;           //sera appelée par Edge Impulse pour récupérer les échantillons
    
    // Exécuter le classifieur
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);         // prend le signal audio en entrée et retourne les probabilités pour chaque classe
    
    if (err != EI_IMPULSE_OK) {
        Serial.print("❌ Erreur classification: ");
        Serial.println(err);
        return;
    }
    
    // Récupération des scores
    float on_score = 0.0;
    float off_score = 0.0;
    float noise_score = 0.0;
    
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        String label = String(result.classification[i].label);
        float value = result.classification[i].value;
        
        if (label == "on") {
            on_score = value;
        } else if (label == "off") {
            off_score = value;
        } else if (label == "noise") {
            noise_score = value;
        }
    }
    
    // Affichage dans le moniteur série
    Serial.print("ON: ");
    Serial.print(on_score, 3);
    Serial.print("  |  OFF: ");
    Serial.print(off_score, 3);
    Serial.print("  |  noise: ");
    Serial.print(noise_score, 3);
    
    // Décision avec seuil de confiance à 70%
    const float THRESHOLD = 0.70;
    String prediction = "";
    float confidence = 0.0;
    
    if (on_score > THRESHOLD && on_score > off_score) {
        digitalWrite(LED_PIN, HIGH);
        prediction = "ON";
        confidence = on_score;
        Serial.print("  → 🔥 ON DÉTECTÉ - LED ALLUMÉE 🔥");
    }
    else if (off_score > THRESHOLD && off_score > on_score) {
        digitalWrite(LED_PIN, LOW);
        prediction = "OFF";
        confidence = off_score;
        Serial.print("  → 🔥 OFF DÉTECTÉ - LED ÉTEINTE 🔥");
    }
    else {
        prediction = "NONE";
        confidence = max(on_score, max(off_score, noise_score));
        Serial.print("  → ⏺ Aucune commande claire");
    }
    Serial.println();
    
    // Envoyer à ThingSpeak si la prédiction a changé ou si on dépasse l'intervalle
    if (prediction != last_prediction || 
        (millis() - last_publish_time > PUBLISH_INTERVAL_MS && prediction != "NONE")) {
        
        if (prediction == "ON" || prediction == "OFF") {
            publishToThingSpeak(prediction, confidence);
            last_prediction = prediction;
            last_confidence = confidence;
            last_publish_time = millis();
        }
    }
}

// ========== FONCTION REQUISE PAR EDGE IMPULSE ==========
// Fournit les données audio au classifieur
int get_audio_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        if (offset + i < SAMPLES_PER_WINDOW) {
            // Normalisation int16 → float (entre -1 et 1)
            out_ptr[i] = (float)audio_buffer[offset + i] / 32768.0f;
        } else {
            out_ptr[i] = 0.0f;
        }
    }
    return EIDSP_OK;
}
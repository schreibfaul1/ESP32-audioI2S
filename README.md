# ESP32-audioI2S

:warning: **This library only works on multi-core chips like ESP32, ESP32-S3 and ESP32-P4. Your board must have PSRAM! It does not work on the ESP32-S2, ESP32-C3 etc** :warning:

***
# Changes applied to ESP32-audioI2S (v3.4.4d)

This document summarizes the modifications, improvements, and bug fixes applied to the library to optimize resource usage and clean up system logs.

## 1. Modular Codec Selection
**Goal:** Reduce Flash and RAM footprint by compiling only the necessary codecs.

- **Implementation:** Added `#define` macros at the top of `Audio.h` for each supported codec (`MP3`, `AAC`, `M4A`, `FLAC`, `WAV`, `OPUS`, `VORBIS`, `OGG`).
- **Dependencies:** Added automatic logic to ensure `AUDIO_CODEC_AAC` is enabled if `AUDIO_CODEC_M4A` is selected.
- **Selective Compilation:** 
    - Wrapped decoder class inclusions and instantiations in `Audio.cpp` with `#ifdef` blocks.
    - Conditionalized large metadata structures (`m_ID3Hdr`, `m_m4aHdr`, `m_rflh`, `m_rwh`) in the `Audio` class.
    - Maintained `read_ID3_Header` availability if either `MP3` or `AAC` is active to support HLS metadata.

## 2. Dynamic PSRAM Buffer Configuration
**Goal:** Allow users to adjust the network buffer size without modifying the library source code.

- **AudioBuffer Class:** Added `setBufsize(size_t ram, size_t psram)` to allow updating internal size variables.
- **Audio Class:** Added `setConnectionBuffSize(size_t size)` as a public method.
- **Feedback:** The library now logs the buffer resize event (e.g., `PSRAM Buffer resized: 655350 -> 1000000 bytes`) via the `audio_info` callback.

## 3. I2S State Management & Log Cleanup
**Goal:** Eliminate the annoying `E (10013) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet` error in the serial console.

- **State Tracking:** Added a private boolean member `m_i2s_enabled` to the `Audio` class.
- **Logic:** 
    - `I2Sstart()` only enables the channel if it's currently disabled.
    - `I2Sstop()` only disables the channel if it's currently enabled.
    - Updated `reconfigI2S()`, `resampleTo48kStereo()`, and the `~Audio()` destructor to respect this flag.
- **Result:** System logs are now clean during startup and station changes.

## 4. Modular Speech Features (TTS)
**Goal:** Remove dependencies on the Arduino `String` class and save Flash by disabling unused TTS features.

- **Implementation:** Added `AUDIO_ENABLE_SPEECH` macro in `Audio.h`.
- **Refactoring:** Completely rewrote `openai_speech()` to use `const char*` and `ps_ptr` instead of `String`, ensuring better memory management.
- **Selective Compilation:** Wrapped `openai_speech()` and `connecttospeech()` methods, as well as the `m_speechtxt` member, with `#ifdef` blocks.

## 5. Modular File System Support
**Goal:** Remove local file system dependencies (SD, FFat, etc.) for pure WebRadio projects.

- **Implementation:** Added `AUDIO_ENABLE_FS` macro.
- **Selective Compilation:** Wrapped all file system related includes (`FS.h`, `SD.h`...) and methods (`connecttoFS`, `processLocalFile`, `fsRange`) with `#ifdef` blocks.

## 6. Granular Log Control
**Goal:** Save Flash by physically removing log strings during compilation.

- **Implementation:** Added `AUDIO_LOG_LEVEL` macro in `Audio.h`.
- **Levels:** `0: None`, `1: Error`, `2: Warning`, `3: Info`, `4: Debug`.
- **Nuance:** This setting only affects internal library logs (`AUDIO_LOG_XXX` macros). Functional data sent to your `myAudioInfo` callback (station name, bitrate, stream title) remains active, as these are functional events rather than simple logs.

***

# Changements appliqués à ESP32-audioI2S (v3.4.4d)

Ce document résume les modifications, améliorations et corrections apportées à la bibliothèque pour optimiser l'utilisation des ressources et nettoyer les journaux (logs) système.

## 1. Sélection Modulaire des Codecs
**Objectif :** Réduire l'empreinte Flash et RAM en ne compilant que les codecs nécessaires.

- **Implémentation :** Ajout de macros `#define` au début de `Audio.h` pour chaque codec supporté (`MP3`, `AAC`, `M4A`, `FLAC`, `WAV`, `OPUS`, `VORBIS`, `OGG`).
- **Dépendances :** Ajout d'une logique automatique pour activer `AUDIO_CODEC_AAC` si `AUDIO_CODEC_M4A` est sélectionné.
- **Compilation Sélective :** 
    - Les inclusions et instantiations des classes de décodeurs dans `Audio.cpp` sont désormais encadrées par des blocs `#ifdef`.
    - Les structures de données volumineuses pour les métadonnées (`m_ID3Hdr`, `m_m4aHdr`, `m_rflh`, `m_rwh`) sont conditionnelles dans la classe `Audio`.
    - La fonction `read_ID3_Header` reste disponible si `MP3` ou `AAC` est actif pour supporter les métadonnées HLS.

## 2. Configuration Dynamique du Buffer PSRAM
**Objectif :** Permettre à l'utilisateur d'ajuster la taille du tampon réseau sans modifier le code source de la bibliothèque.

- **Classe AudioBuffer :** Ajout de `setBufsize(size_t ram, size_t psram)` pour permettre la mise à jour des variables de taille internes.
- **Classe Audio :** Ajout de la méthode publique `setConnectionBuffSize(size_t size)`.
- **Retour d'information :** La bibliothèque enregistre maintenant l'événement de redimensionnement (ex: `PSRAM Buffer resized: 655350 -> 1000000 bytes`) via le callback `audio_info`.

## 3. Gestion d'État I2S et Nettoyage des Logs
**Objectif :** Éliminer l'erreur `E (10013) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet` dans la console série.

- **Suivi d'État :** Ajout d'un membre booléen privé `m_i2s_enabled` à la classe `Audio`.
- **Logique :** 
    - `I2Sstart()` n'active le canal que s'il est actuellement désactivé.
    - `I2Sstop()` ne désactive le canal que s'il est actuellement activé.
    - Mise à jour de `reconfigI2S()`, `resampleTo48kStereo()`, et du destructeur `~Audio()` pour respecter ce flag.
- **Résultat :** Les journaux système sont propres lors du démarrage et des changements de station.

## 4. Modularité des Fonctions Vocales (TTS)
**Objectif :** Supprimer la dépendance à la classe `String` d'Arduino et économiser de la Flash en désactivant les fonctions vocales inutilisées.

- **Implémentation :** Ajout de la macro `AUDIO_ENABLE_SPEECH` dans `Audio.h`.
- **Refactorisation :** Réécriture de `openai_speech()` pour utiliser `const char*` et `ps_ptr` à la place de `String`, garantissant une meilleure gestion de la mémoire.
- **Compilation Sélective :** Les méthodes `openai_speech()` et `connecttospeech()`, ainsi que le membre `m_speechtxt`, sont désormais conditionnels.

## 5. Modularité du Système de Fichiers (FS)
**Objectif :** Supprimer les dépendances aux fichiers locaux (SD, FFat, etc.) pour les projets purement WebRadio.

- **Implémentation :** Ajout de la macro `AUDIO_ENABLE_FS`.
- **Compilation Sélective :** Toutes les inclusions (`FS.h`, `SD.h`...) et les méthodes liées aux fichiers (`connecttoFS`, `processLocalFile`) sont encadrées par des blocs `#ifdef`.

## 6. Contrôle Granulaire des Logs
**Objectif :** Économiser de la Flash en supprimant physiquement les chaînes de caractères des logs lors de la compilation.

- **Implémentation :** Ajout de la macro `AUDIO_LOG_LEVEL` dans `Audio.h`.
- **Niveaux :** `0: Aucun`, `1: Erreur`, `2: Avertissement`, `3: Info`, `4: Debug`.
- **Nuance :** Ce réglage n'affecte que les logs internes de la bibliothèque (macros `AUDIO_LOG_XXX`). Les informations envoyées à votre callback `myAudioInfo` (nom de station, bitrate, titre du flux) restent actives car il s'agit d'événements fonctionnels et non de simples logs.

***

Plays mp3, m4a and wav files from SD card via I2S with external hardware.
HELIX-mp3 and faad2-aac decoder is included. There is also an OPUS decoder for Fullband, an VORBIS decoder and a FLAC decoder.
Works with MAX98357A (3 Watt amplifier with DAC), connected three lines (DOUT, BLCK, LRC) to I2S. The I2S output frequency is always 48kHz, regardless of the input source, so Bluetooth devices can also be connected without any problems.
For stereo are two MAX98357A necessary. AudioI2S works with UDA1334A (Adafruit I2S Stereo Decoder Breakout Board), PCM5102A and CS4344.
Other HW may work but not tested. Plays also icy-streams, GoogleTTS and OpenAIspeech. Can be compiled with Arduino IDE. [WIKI](https://github.com/schreibfaul1/ESP32-audioI2S/wiki)

```` c++
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"

// Digital I/O used
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

String ssid =     "*******";
String password = "*******";

Audio audio;

// callbacks
void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Audio::audio_info_callback = my_audio_info; // optional
    Serial.begin(115200);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // default 0...21
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/");
}

void loop(){
    audio.loop();
    vTaskDelay(1);
}

````
You can find more examples here: https://github.com/schreibfaul1/ESP32-audioI2S/tree/master/examples

````c++
// detailed cb output
void my_audio_info(Audio::msg_t m) {
    switch(m.e){
        case Audio::evt_info:           Serial.printf("info: ....... %s\n", m.msg); break;
        case Audio::evt_eof:            Serial.printf("end of file:  %s\n", m.msg); break;
        case Audio::evt_bitrate:        Serial.printf("bitrate: .... %s\n", m.msg); break; // icy-bitrate or bitrate from metadata
        case Audio::evt_icyurl:         Serial.printf("icy URL: .... %s\n", m.msg); break;
        case Audio::evt_id3data:        Serial.printf("ID3 data: ... %s\n", m.msg); break; // id3-data or metadata
        case Audio::evt_lasthost:       Serial.printf("last URL: ... %s\n", m.msg); break;
        case Audio::evt_name:           Serial.printf("station name: %s\n", m.msg); break; // station name or icy-name
        case Audio::evt_streamtitle:    Serial.printf("stream title: %s\n", m.msg); break;
        case Audio::evt_icylogo:        Serial.printf("icy logo: ... %s\n", m.msg); break;
        case Audio::evt_icydescription: Serial.printf("icy descr: .. %s\n", m.msg); break;
        case Audio::evt_image: for(int i = 0; i < m.vec.size(); i += 2){
                                        Serial.printf("cover image:  segment %02i, pos %07lu, len %05lu\n", i / 2, m.vec[i], m.vec[i + 1]);} break; // APIC
        case Audio::evt_lyrics:         Serial.printf("sync lyrics:  %s\n", m.msg); break;
        case Audio::evt_log   :         Serial.printf("audio_logs:   %s\n", m.msg); break;
        default:                        Serial.printf("message:..... %s\n", m.msg); break;
    }
}
````
<br>

|Codec       | ESP32       |ESP32-S3 or ESP32-P4         |                          |
|------------|-------------|-----------------------------|--------------------------|
| mp3        | y           | y                           |                          |
| aac        | y           | y                           |                          |
| aacp       | y (mono)    | y (+SBR, +Parametric Stereo)|                          |
| wav        | y           | y                           |                          |
| flac       | y           | y                           |blocksize max 24576 bytes |
| vorbis     | y           | y                           | <=196Kbit/s              |
| m4a        | y           | y                           |                          |
| opus       | y           | y                           |hybrid mode not impl yet  |

<br>

***
Wiring
![Wiring ESP32-S3](https://github.com/user-attachments/assets/15dd1766-0fc1-4079-b378-bc566583e80d)
***
Impulse diagram
![Impulse diagram](https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/additional_info/Impulsdiagramm.jpg)
***
Yellobyte has developed an all-in-one board. It includes an ESP32-S3 N8R2, 2x MAX98357 and an SD card adapter.
Documentation, circuit diagrams and examples can be found here: https://github.com/yellobyte/ESP32-DevBoards-Getting-Started
![image](https://github.com/user-attachments/assets/4002d09e-8e76-4e08-9265-188fed7628d3)


# ESP32-audioI2S

:warning: **This library only works on multi-core chips like ESP32, ESP32-S3 and ESP32-P4. Your board must have PSRAM! It does not work on the ESP32-S2, ESP32-C3 etc** :warning:

***
# Changes applied to ESP32-audioI2S (v3.4.4d)

This document provides a detailed breakdown of the modifications, improvements, and bug fixes applied to the library. Each change aims to optimize resource usage (Flash, RAM, CPU) and improve system stability.

## 1. Modular Codec Selection
**Context:** By default, the library compiles all decoders (MP3, AAC, FLAC, WAV, OPUS, VORBIS, OGG), consuming significant Flash memory even if only one format is used.
**Improvement:**
- Added `#define` macros in `Audio.h` to selectively enable/disable each codec.
- Implemented automatic dependency logic (e.g., enabling `AUDIO_CODEC_M4A` forces `AUDIO_CODEC_AAC`).
- Wrapped decoders and their large metadata structures (`m_ID3Hdr`, etc.) with conditional compilation blocks.
**Benefit:** Drastic reduction in Flash usage (up to ~300KB saved) and static RAM allocation.

## 2. Dynamic PSRAM Buffer Configuration
**Context:** The network buffer size was hardcoded (~640KB), forcing a recompile to change it.
**Improvement:**
- Added `setBufsize(size_t ram, size_t psram)` to the `AudioBuffer` class.
- Added `setConnectionBuffSize(size_t size)` to the `Audio` class to adjust the buffer size at runtime (before connection).
**Benefit:** Allows fine-tuning the buffer size based on available PSRAM and network stability without modifying the library source.

## 3. I2S State Management & Log Cleanup
**Context:** The library often generated `E (10013) i2s_channel_disable: channel not enabled` errors during startup or station changes, spamming the logs.
**Improvement:**
- Added `m_i2s_enabled` flag to track the actual state of the I2S channel.
- Modified `I2Sstart()` and `I2Sstop()` to check this flag before calling ESP-IDF drivers.
**Benefit:** Clean, error-free system logs, making real issues easier to spot.

## 4. Modular Speech Features (TTS) & String Elimination
**Context:** The TTS features (Google/OpenAI) heavily relied on the Arduino `String` class, causing heap fragmentation through dynamic concatenations.
**Improvement:**
- Added `AUDIO_ENABLE_SPEECH` macro to completely disable TTS code if unused.
- **Refactoring:** Completely rewrote `openai_speech()` to use `const char*`, `ps_ptr` buffers, and `snprintf`.
**Benefit:**
- Saves Flash when TTS is disabled.
- Eliminates heap fragmentation risks by removing the `String` class dependency from the TTS module.

## 5. Modular File System Support
**Context:** The library unconditionally included libraries for SD, SD_MMC, FFat, and LittleFS, which is unnecessary for pure WebRadio projects.
**Improvement:**
- Added `AUDIO_ENABLE_FS` macro.
- Wrapped all FS-related includes and methods (`connecttoFS`, `processLocalFile`, `fsRange`) with `#ifdef` blocks.
**Benefit:** Removes dependencies on external FS libraries, speeding up compilation and ensuring a cleaner build for web-only projects.

## 6. Granular Log Control
**Context:** Debug strings take up valuable Flash memory.
**Improvement:**
- Added `AUDIO_LOG_LEVEL` macro (0=None to 4=Debug).
- Redefined log macros to strip format strings from the binary at compilation time based on the level.
**Benefit:** Saves ~10-15KB of Flash by removing static debug strings in production builds.

## 7. Performance & Memory Optimizations
**Context:**
- The IIR filter chain (Equalizer) was consuming CPU cycles for every sample even when gains were zero.
- Internal string arrays were consuming RAM.
- `resampleTo48kStereo` was reallocating a `std::vector` for every audio chunk.
**Improvement:**
- **Dynamic DSP Bypass:** Skips the filter chain calculations if all gains are zero.
- **Static Constexpr:** Moved internal lookup tables (`plsFmtStr`, etc.) to Flash (PROGMEM).
- **Persistent Resample Buffer:** Implemented a persistent `ps_ptr` buffer for resampling to avoid repetitive heap allocations.
- **STL Removal:** Replaced remaining `std::string` usage with C-strings.
**Benefit:** Significant CPU savings, reduced RAM usage, and reduced stress on the memory allocator.

***

# Changements appliqués à ESP32-audioI2S (v3.4.4d)

Ce document détaille les modifications, améliorations et corrections apportées à la bibliothèque. Chaque changement vise à optimiser l'utilisation des ressources (Flash, RAM, CPU) et à améliorer la stabilité du système.

## 1. Sélection Modulaire des Codecs
**Contexte :** Par défaut, la bibliothèque compile tous les décodeurs (MP3, AAC, FLAC, WAV, etc.), consommant beaucoup de mémoire Flash même si un seul format est utilisé.
**Amélioration :**
- Ajout de macros `#define` dans `Audio.h` pour activer/désactiver chaque codec.
- Gestion automatique des dépendances (ex: activer M4A active automatiquement AAC).
- Encadrement conditionnel des structures de métadonnées volumineuses.
**Bénéfice :** Réduction drastique de l'empreinte Flash (jusqu'à ~300 Ko économisés) et de l'allocation RAM statique.

## 2. Configuration Dynamique du Buffer PSRAM
**Contexte :** La taille du tampon réseau était fixée en dur (~640 Ko), obligeant à recompiler pour la modifier.
**Amélioration :**
- Ajout de la méthode publique `setConnectionBuffSize(size_t size)`.
**Bénéfice :** Permet d'ajuster finement la taille du tampon au démarrage en fonction de la PSRAM disponible et de la qualité du réseau, sans modifier le code de la bibliothèque.

## 3. Gestion d'État I2S et Nettoyage des Logs
**Contexte :** Des erreurs `E (10013)` apparaissaient fréquemment dans les logs car la bibliothèque tentait d'arrêter un canal I2S déjà arrêté.
**Amélioration :**
- Ajout d'un flag `m_i2s_enabled` pour suivre l'état réel du canal.
- Les fonctions `I2Sstart()` et `I2Sstop()` vérifient désormais cet état avant d'agir.
**Bénéfice :** Des logs système propres, facilitant le diagnostic des vrais problèmes.

## 4. Modularité TTS et Suppression de String
**Contexte :** Les fonctions de synthèse vocale utilisaient la classe Arduino `String`, provoquant une fragmentation de la mémoire (tas/heap) à cause des concaténations dynamiques.
**Amélioration :**
- Ajout de la macro `AUDIO_ENABLE_SPEECH` pour désactiver complètement le TTS.
- **Refactorisation :** Réécriture complète de `openai_speech()` en utilisant des `const char*` et `snprintf` avec des buffers `ps_ptr`.
**Bénéfice :** Élimination des risques de fragmentation mémoire et économie de Flash si le module est désactivé.

## 5. Modularité du Système de Fichiers (FS)
**Contexte :** La bibliothèque incluait systématiquement les librairies SD, FFat, etc., inutile pour une WebRadio pure.
**Amélioration :**
- Ajout de la macro `AUDIO_ENABLE_FS` pour exclure tout le code lié aux fichiers locaux.
**Bénéfice :** Supprime les dépendances inutiles, accélère la compilation et allège le binaire.

## 6. Contrôle Granulaire des Logs
**Contexte :** Les chaînes de caractères de débogage occupent de la place en Flash.
**Amélioration :**
- Ajout de `AUDIO_LOG_LEVEL` (0 à 4) pour filtrer les logs à la compilation.
**Bénéfice :** Économie de 10 à 15 Ko de Flash en supprimant les textes de debug en production.

## 7. Optimisations Performance & Mémoire
**Contexte :**
- L'égaliseur (IIR) consommait du CPU même avec des gains à zéro.
- Les tableaux de chaînes internes étaient stockés en RAM.
- Le ré-échantillonnage 48kHz réallouait un vecteur à chaque cycle.
**Amélioration :**
- **Bypass DSP :** Saut automatique des calculs de filtres si les gains sont nuls.
- **Constexpr :** Déplacement des tableaux de chaînes en Flash (PROGMEM).
- **Buffer Persistant :** Utilisation d'un buffer `ps_ptr` unique pour le resampling.
- **Suppression STL :** Remplacement des derniers `std::string` par des chaînes C.
**Bénéfice :** Gain significatif en cycles CPU, réduction de l'usage RAM et soulagement de l'allocateur mémoire.

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

```
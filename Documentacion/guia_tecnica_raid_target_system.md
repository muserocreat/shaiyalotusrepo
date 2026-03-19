# Guía Técnica: Sistema de Raid Target Broadcaster (Shaiya Lotus)

Este documento detalla la implementación del sistema de anuncio automático de objetivos para líderes de Raid, diseñado específicamente para ser 100% estable y evitar los crashes históricos en la creación de personajes en el Core de Shaiya Lotus (EP6.4).

---

## 🔬 Arquitectura del Sistema: Del Cliente al Servidor

El sistema funciona en bucle cerrado: el cliente detecta el click del líder y envía un comando secreto al servidor, el cual lo procesa y lo retransmite a toda la Raid.

### 1. Cliente: El "Vigía" (Detección de Objetivo)
Interceptor de selección de objetivos en tiempo real.

- **Hook de Control**: `hook::target()` en el cliente.
- **Dirección de Hook**: `0x44CECC` (EP6.4 Client).
- **Registro Crítico**: `ESI` (Contiene el puntero al objetivo actual).
- **Lógica de Filtrado**:
  - Solo actúa si el jugador es el **Líder de la Party/Raid** (`0x44CF14`).
  - Filtra objetivos por rango y facción contraria (opcional).
  - Envía el comando `!!target` al servidor mediante el chat normal.

### 2. Servidor: El "Filtro de Guante Blanco" (Hook de Tabla)
A diferencia de los Hooks convencionales (Detours), este sistema usa un **Hook de Tabla de Dispatcher** para garantizar la estabilidad total.

- **Dispatcher de Grupo 0x11 (Chat)**:
  - **Dirección de la Tabla**: `0x47FC80`.
  - **Puntero de Chat Normal (Índice 1)**: `0x47FC84`.
- **Funcionamiento**: 
  1. Sobrescribimos el valor en `0x47FC84` con nuestra función `naked_chat_proxy`.
  2. El servidor llama a nuestra función como si fuera la oficial.
  3. No modificamos ninguna instrucción del ejecutable `ps_game.exe`.

- **Registros Maestros de Shaiya Lotus**:
  - **EBX**: Puntero al paquete entrante (GameChatNormalIncoming).
  - **ECX**: Puntero al usuario (CUser).
- **Dirección Original de Chat**: `0x47F5F2` (Usada para saltar de vuelta cuando el mensaje no es `!!`).

---

## 🛡️ Estabilidad y Prevención de Crashes

### ¿Por qué Lotus crasheaba antes?
Lotus intentaba hookear la dirección `0x47A1F0` con 6 bytes. 
1. Esa dirección era el **final** de una función, no el principio. 
2. Al poner un salto ahí, se rompía el mecanismo de salida (Epílogo) de la función.
3. Cuando el servidor procesaba la creación de personajes, pasaba por esa zona dañada y estallaba (Excepción `0xC0000005`).

### ¿Por qué este sistema es Indestructible?
Al usar un **Hook de Tabla (`0x47FC84`)**, el servidor no sufre cambios en su código binario. Solo cambiamos la "etiqueta" de una dirección. Esto hace que la creación de personajes y el login sean totalmente seguros, ya que el Patch solo actúa cuando el servidor recibe un paquete de chat explícitamente.

---

## 🚀 Guía de Uso Manual

Además de la automatización del cliente, el líder de la Raid puede disparar el sistema manualmente:

- **`!!target`**: Anuncia el objetivo actual del líder (Jugador o Mob).
- **`!![Mensaje]`**: Envía un aviso de banda (Raid Warning) a todos los miembros de la Unión/Banda.

---

## 🛠️ Herramientas de Mantenimiento
Si en el futuro cambias el ejecutable `ps_game.exe`, el patrón para encontrar la tabla de chat es:
`83 F8 11 ... FF 24 85 80 FC 47 00` (Dispatcher Group 0x11).

Implementado con éxito en el Core **Shaiya Lotus** (Marzo 2026).

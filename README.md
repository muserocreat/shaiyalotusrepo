# Shaiya Lotus PS Source

**Shaiya Lotus PS Source** es un proyecto de código abierto diseñado para proporcionar una base universal para servidores privados de Shaiya. Este repositorio contiene el código utilizado por el servidor (PSGame), login (PSLogin), agente de base de datos (PSDBAgent) y el cliente (Game.exe).

Esta versión personalizada de **Shaiya Lotus PS Source** ha sido optimizada para ofrecer **Estabilidad Industrial** y nuevas funcionalidades avanzadas, corrigiendo errores heredados de versiones anteriores (como el crash de creación de personajes en Lotus).

---

---

## 🛠️ Sistemas Dinámicos (Live Sync)
El núcleo de **Shaiya Lotus** utiliza una arquitectura de sincronización en tiempo real con SQL Server, permitiendo modificaciones en caliente sin necesidad de reinicios (Hot-Reload).

### 1. Sistema de Drops Dinámicos (Dynamic Drop)
Gestor de drops de alto rendimiento que inyecta datos directamente en la memoria del servidor (`ps_game.exe`).
*   **Conexión Nativa**: Comunicación directa mediante ODBC con la base de datos `PS_GameDefs`.
*   **Shadow Buffering**: Utiliza un búfer secundario para procesar cambios, garantizando que el servidor nunca sufra "lag" o bloqueos durante la recarga de miles de mobs.
*   **Trigger de Recarga**: Monitorea la tabla `ServerControl` cada 10 segundos. Si detecta la señal `ReloadDrops`, actualiza las tablas de ítems de forma atómica.
*   **Logging**: Registro detallado de cada cambio en `Data\DropChanged.ini`.

### 2. Tienda en Vivo (Dynamic Mall)
Permite modificar el inventario de la tienda de ítems (Item Mall) y los precios de forma instantánea.
*   **Capacidad Extendida**: Soporte nativo para hasta 24 slots de ítems por cada paquete (ProductCode).
*   **Sincronización de Precios**: Actualización instantánea de los costes en AP (Aeria Points) reflejada en el cliente sin reloguear.
*   **Mapeo de Memoria**: Inyección directa en `CGameData::ProductInfo`.

### 3. Recarga de Puntos Automática (Dynamic Point)
Sistema de actualización de saldo (AP) en tiempo real para los jugadores.
*   **Cola de Recarga**: Escanea la tabla `PointReloadQueue` cada 3 segundos.
*   **Refresco Invisible**: Envía paquetes de sincronización al **DBAgent** para actualizar el saldo del usuario mientras este sigue jugando, ideal para integraciones con paneles web de donaciones.

---

## 🎭 Identidad Visual: Cloak Name Colors
Un sistema avanzado de personalización estética implementado en el cliente (`Game.exe`) mediante hooks de ensamblador.

### Jerarquía de Prioridades
El sistema evalúa el equipamiento del jugador en tiempo real para determinar el color de su nombre:

1.  **Staff & GM (Efectos Dinámicos)**:
    *   **Cloak ID 24110**: Efecto **Quad Rainbow** (Ciclo discreto entre Rojo, Amarillo, Verde y Naranja).
    *   **Cloak ID 24109/24114**: Efecto **Smooth Rainbow** (Ciclo HSL fluido con velocidad ajustable).
2.  **Títulos VIP (Colores Estáticos)**:
    *   Asignación de colores premium (Turquesa, DodgerBlue, SlateBlue) basados en IDs específicos de capas VIP.
3.  **Sistema de Cascos (Rango de Ítem)**:
    *   Si no hay una capa especial, el color se determina por el valor `range` del casco equipado (10 variantes de color, desde Celeste hasta Negro).

### Detalles Técnicos
*   **Hook de Renderizado**: Se interceptan las direcciones de memoria de dibujo de nombres (ej. `0x4537D5`) para inyectar el color personalizado antes de que el motor de DirectX procese el texto.
*   **Seguridad de Pila**: Implementación con `pushad`/`popad` y ajustes manuales de `esp` para garantizar compatibilidad con cualquier parche de FPS o UI.

---

## 🎯 Marcado de Objetivos (Automatic Target Broadcaster)
Sistema de coordinación táctica en tiempo real para líderes de unión en entornos PvP masivos.

*   **Detección Automática**: El cliente intercepta el ID del objetivo seleccionado por el **Boss de la Unión** (Líder) en la dirección `0x44CF14`.
*   **Aviso de Banda (Notification)**: Si el líder marca a un enemigo, el sistema envía automáticamente un mensaje de alerta a toda la unión: `>>> ATTACK: [Nombre] <<<`.
*   **Filtros de Combate**:
    *   **Zonas de Guerra**: Solo se activa en mapas PvP (Proelium, Cantabilian, Deep Desert, Kanos Illu, etc.).
    *   **Anti-Spam**: Incluye un sistema de *Debounce* de 500ms para evitar saturar el chat durante el cambio rápido de objetivos.
    *   **Validación de Alianzas**: El sistema ignora automáticamente a aliados y NPCs para centrarse solo en jugadores de la facción enemiga.

---

## ⚔️ Mecánicas de Juego (Game Logic)

### Bloqueo de Jefes (Boss Party Lock)
Fomenta la cooperación de banda impidiendo que jugadores individuales o grupos pequeños "soleen" jefes mundiales.
*   **Requisito de Grupo**: Validación de mínimo 3 miembros en la party/raid para infligir daño.
*   **Validación de Drops**: Integrado con el sistema Dynamic Drop para asegurar que los ítems solo caigan si se cumplieron las reglas de banda al momento de la muerte del jefe.

---

## 📑 Documentación Técnica
- [Guía Técnica: Raid Target System](file:///c:/Users/Maxi/Desktop/Principal/Shaiya-Core-main/Documentacion/guia_tecnica_raid_target_system.md)

---

### 🏛️ Créditos
- **[DEV]Masi0**: Desarrollador Server Side 
- **[DEV]Tonchi**: Desarrollador Web - Sistemas Automatizacion
- **Lotus Customizations**: Desarrollado para garantizar un entorno estable y dinámico en servidores de producción de alta carga.

---

⚠ **Aviso**: Esta versión del código requiere SQL Server con el driver ODBC configurado para el usuario `lotus`.

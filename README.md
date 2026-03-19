# Shaiya-Core (Modificado - Lotus Edition)

Core es un proyecto de código abierto diseñado para proporcionar una base universal para servidores privados de Shaiya. Este repositorio contiene el código utilizado por el servidor (PSGame), login (PSLogin), agente de base de datos (PSDBAgent) y el cliente (Game.exe).

Esta versión personalizada del proyecto Core ha sido optimizada para ofrecer **Estabilidad Industrial** y nuevas funcionalidades avanzadas, corrigiendo errores heredados de versiones anteriores (como el crash de creación de personajes en Lotus).

---

## 🚀 Nuevas Funcionalidades Implementadas

### 1. Sistema de Raid Target Broadcaster (Automático)
Un sistema de comunicación táctica para bandas y uniones que permite al líder marcar objetivos de forma automática y manual.
*   **Automatización**: El cliente detecta cuando el líder selecciona un nuevo objetivo enemigo en PvP y envía la señal al servidor.
*   **Aviso de Banda (!!)**: Soporte nativo para anuncios de banda con el prefijo `!!`.
*   **Estabilidad Superior**: Implementado mediante un **Hook de Tabla de Dispatcher** (0x47FC84), garantizando que el sistema sea 100% independiente del proceso de creación de personajes y login.

### 2. Sistema de Drops Dinámicos (Dynamic Drop)
Gestor de drops flexible y de alto rendimiento basado en archivos JSON.
*   **Carga en Caliente**: Permite recargar las tablas de drops sin reiniciar el servidor.
*   **Integración de Logros**: Preparado para trabajar en conjunto con sistemas de misiones y recompensas dinámicas.

### 3. Sistema de Bloqueo de Jefes (Boss Party Lock)
Un mecanismo para prevenir el "soling" de jefes mundiales, fomentando el juego en equipo.
*   **Requisito de Party**: Los jefes específicos solo reciben daño si el atacante forma parte de un grupo de al menos 3 personas.
*   **Validación de Muerte**: Control preciso de la distribución de drops en función del cumplimiento de los requisitos de banda.

---

## 🛡️ Estabilidad y Rendimiento

Este proyecto prioriza la robustez del servidor sobre la cantidad de modificaciones:
- **Prevención de Crashes**: Se ha implementado un parche de estabilidad en la capa de red y chat para evitar desbordamientos de memoria y punteros nulos durante picos de carga.
- **Memoria Optimizada**: Uso de acceso directo por offsets en lugar de enums incompletos, asegurando la compatibilidad con ejecutables `ps_game.exe` modificados.

---

## 📑 Documentación Técnica
Para más detalles sobre la implementación de los ganchos de memoria (Hooks), offsets y registros utilizados, consulte la carpeta `/Documentacion`:
- [Guía Técnica: Raid Target System](file:///c:/Users/Maxi/Desktop/Principal/Shaiya-Core-main/Documentacion/guia_tecnica_raid_target_system.md)

---

### 🏛️ Créditos y Orígenes
- **Base Server/Login/Agent**: Basado en [Shaiya Episode 6](https://github.com/kurtekat/shaiya-episode-6)
- **Base Client Code**: Basado en [Shaiya Essentials](https://github.com/Spelunkern/shaiya-essentials)
- **Lotus Customizations**: Implementadas para garantizar un entorno estable de producción.

---

⚠ **Aviso**: Esta versión del repositorio está optimizada para servidores Lotus y puede requerir ajustes específicos para otros ejecutables binarios.

# Resumen de Sistemas Implementados (Lotus Edition)

Este documento proporciona una visión general de las mejoras y nuevos sistemas añadidos al Core de Shaiya para garantizar la estabilidad y experiencia de juego en los servidores Lotus.

---

## 1. Sistema de Raid Target Broadcaster (Automatizado)
Un sistema de comunicación táctica integrado entre el cliente y el servidor.

- **Objetivo**: Permitir que el líder de una banda/unión marque objetivos de ataque automáticamente para todos sus miembros.
- **Implementación**:
    - **Cliente**: Hook en `0x44CECC` para capturar la selección de objetivos. Envía un comando secreto `!!target` al servidor.
    - **Servidor**: Hook de Tabla de Dispatcher en `0x47FC84`. Intercepta los comandos `!!` y los retransmite a través del Chat de Unión (`0x1112`).
- **Seguridad**: Usa el mecanismo de **Tabla de Punteros**, evitando desplazar código original del servidor. No causa crashes en la creación de personajes.

## 2. Sistema de Drops Dinámicos (Dynamic Drop)
Gestión avanzada del botín de monstruos y jefes mediante JSON.

- **Objetivo**: Poder cambiar el drop de cualquier monstruo sin tener que editar el ejecutable ni reiniciar el servidor.
- **Implementación**:
    - Se integra en `CMob::OnDeath` (Hook `0x4656F7` en el Dispatcher).
    - Lee las tablas desde `Drop.json` configurado en la carpeta de datos.
- **Ventaja**: Permite eventos temporales de drops (como eventos de Navidad o PvP) simplemente cambiando un archivo y recargando con un comando.

## 3. Sistema de Bloqueo de Jefes (Boss Party Lock)
Regulación del enfrentamiento contra jefes mundiales.

- **Objetivo**: Evitar que un solo jugador (con equipo superior) monopolice los jefes que deberían ser un reto de banda.
- **Implementación**:
    - Valida el número de miembros en el grupo (`IsPartyMemberCount >= 3`).
    - Si el requisito no se cumple, el jefe no recibe daño o no entrega recompensa.
- **Configuración**: Se puede activar/desactivar y configurar el número mínimo de miembros por jefe.

## 4. Parche de Estabilidad Industrial (Zero-Crash)
Corrección de errores críticos de memoria heredados de proyectos anteriores.

- **Objetivo**: Eliminar los cierres inesperados del servidor durante el login masivo o la creación de personajes.
- **Mejoras**:
    - **Blindaje de Chat**: El servidor ahora valida los punteros del usuario y del paquete antes de procesar el chat.
    - **Acceso Directo por Offsets**: Corregido el problema de los Enums vacíos (como `TargetType`) que causaban errores de punteros nulos.
    - **Corrected Register Convention**: Sincronización exacta con el registro `EBX` para el manejo de paquetes, evitando el desbordamiento del stack.

---

Este conjunto de sistemas convierte al Core en una plataforma lista para servidores de alto tráfico y competitividad.

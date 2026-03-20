#pragma once

namespace shaiya {
    struct CUser;

    /**
     * @brief Clase encargada de gestionar las restricciones de equipamiento
     * para eventos especiales como el Evento Naked en Mapa 60.
     */
    class DynamicNaked {
    public:
        /**
         * @brief Validador de slots para el evento Naked.
         * @param user El usuario que intenta equipar el objeto.
         * @param itemSlot El slot de destino del objeto.
         * @return true si se permite el equipamiento, false si está restringido.
         */
        static bool CanEquip(CUser* user, int itemSlot);
    };
}

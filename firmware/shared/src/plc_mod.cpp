#include "plc_mod.h"
#include "my_func.h"
#ifdef PLC_CPU
#include "stm32_spi.h"
#include "plc_control.h"
#else
#include "plc_mod_info.h"
#endif

#ifdef PLC_CPU
PLC_MOD plc_mod_com;
PLC_MOD plc_mod_ext;
#else
PLC_MOD plc_mod;
#endif

#define PLC_MOD_RESET_TIMEOUT 100
#define PLC_MOD_DATA_TIMEOUT 500
#define PLC_MOD_PKT_PAUSE 50

#ifdef PLC_CPU
void PLC_MOD::init(STM32_SPI *spi, bool is_com)
{
    m_spi = spi;
    m_bus_com = is_com;
}

void PLC_MOD::find_modules()
{
    m_mod_count = 0;

    CS_off();
    STM32_SYSTICK::delay(PLC_MOD_RESET_TIMEOUT);
    CS_on();
    STM32_SYSTICK::delay(PLC_MOD_RESET_TIMEOUT);

    memset((uint8_t*)&m_pkt_send, 0, sizeof(plc_mod_pkt_t));
    m_pkt_send.data_size = 0;
    m_pkt_send.request = EModRequest::GET_INFO;

    PLC_CONTROL::print_message("Get modules info (%s bus):\n", (m_bus_com ? "COM" : "EXT"));

    while (1)
    {
        // set target ID
        m_pkt_send.target_id = m_mod_count;
        m_pkt_recv.request = EModRequest::NONE;

        m_spi->transmit((uint8_t*)&m_pkt_send, sizeof(plc_mod_pkt_t),
                        TXRX_MODE::DIRECT, PLC_MOD_DATA_TIMEOUT);
        STM32_SYSTICK::delay(PLC_MOD_PKT_PAUSE);
        if ((m_spi->transmit_recieve((uint8_t*)&m_pkt_send, (uint8_t*)&m_pkt_recv, sizeof(plc_mod_pkt_t),
                           TXRX_MODE::DIRECT, PLC_MOD_DATA_TIMEOUT) != STM32_RESULT_OK) ||
            (m_pkt_recv.request != EModRequest::GET_INFO))
            break;

        print_module_info(&m_pkt_recv.info);
        // fill module info
        memcpy((uint8_t*)&m_modules[m_mod_count], (uint8_t*)&m_pkt_recv.info,
               sizeof(plc_mod_info_t));

        ++m_mod_count;
    }
    PLC_CONTROL::print_message("Finded %U modules\n", m_mod_count);
}

void PLC_MOD::CS_on()
{
    if (m_bus_com)
        PLC_MOD_COM_SEL_GPIO.pin_OFF(PLC_MOD_COM_SEL_PIN);
    else
        PLC_MOD_EXT_SEL_GPIO.pin_OFF(PLC_MOD_EXT_SEL_PIN);
}

void PLC_MOD::CS_off()
{
    if (m_bus_com)
        PLC_MOD_COM_SEL_GPIO.pin_ON(PLC_MOD_COM_SEL_PIN);
    else
        PLC_MOD_EXT_SEL_GPIO.pin_ON(PLC_MOD_EXT_SEL_PIN);
}

void PLC_MOD::print_module_info(plc_mod_info_t *mod)
{

    PLC_CONTROL::print_message("\tModule: STM32-%08X-%02X:%02X-%02X:%02X\n", mod->type,
                               mod->hw_version, mod->hw_version,
                               mod->inputs, mod->outputs);
}

#else

void PLC_MOD::init(SPI_HandleTypeDef *spi)
{
    m_spi = spi;
}

void PLC_MOD::init_seq()
{
    // wait, while CSin is a HIGH
    while (HAL_GPIO_ReadPin(SEL_IN_GPIO_Port, SEL_IN_Pin) == GPIO_PIN_SET);

    while (!init_seq_get_req())
        HAL_Delay(10);

    init_seq_send_info();
}

bool PLC_MOD::init_seq_get_req()
{
    // recieve packet from CPU
    HAL_SPI_Receive_DMA(m_spi, (uint8_t*)&m_pkt_recv, sizeof(plc_mod_pkt_t));
    // wait, while packet recieved
    while (HAL_SPI_GetState(m_spi) == HAL_SPI_STATE_BUSY_RX);

    if (m_pkt_recv.request == EModRequest::GET_INFO)
        return true;
    else
        return false;
}

void PLC_MOD::init_seq_send_info()
{
    m_pkt_send.target_id = 0;
    m_pkt_send.data_size = sizeof(plc_mod_info_t);
    m_pkt_send.request = EModRequest::GET_INFO;
    memcpy((uint8_t*)&m_pkt_send.info, (uint8_t*)&mod_info, sizeof(plc_mod_info_t));

    if (HAL_SPI_Transmit(m_spi, (uint8_t*)&m_pkt_send, sizeof(plc_mod_pkt_t),
                         PLC_MOD_DATA_TIMEOUT) != HAL_OK)
        Error_Handler();
}

bool PLC_MOD::is_CSin_on()
{
    //
    return true;
}

void PLC_MOD::CSout_on()
{
    //
}

void PLC_MOD::CSout_off()
{
    //
}

#endif

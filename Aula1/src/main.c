#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <pwm_z42.h> // Mantendo sua biblioteca customizada

#define TPM_MODULE 4800
uint16_t duty_50  = TPM_MODULE / 2;
#define SLEEP_TIME_MS   3000

// --- MAPEAMENTO DE PINOS ---
// Caso o PTC12 continue falhando após o teste de software, 
// mude esses valores para pinos de uso geral seguros (ex: 3, 4, 5 e 6).
#define M1_IN1 12 
#define M1_IN2 13 
#define M2_IN1 16 
#define M2_IN2 17 



int main(void)
{
    int ret; // Variável para capturar erros do Zephyr
    const struct device *port_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));

    // Verifica se a porta C está pronta no sistema
    if (!device_is_ready(port_c)) {
        return -1; 
    }

    // 1. Configurar Pinos de Direção com VERIFICAÇÃO DE ERROS
    ret = gpio_pin_configure(port_c, M1_IN1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret; // O Zephyr barra a execução aqui se o pino não puder ser usado

    ret = gpio_pin_configure(port_c, M1_IN2, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret;

    ret = gpio_pin_configure(port_c, M2_IN1, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret;

    ret = gpio_pin_configure(port_c, M2_IN2, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret;

    // 2. Inicializa o TPM2
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_1, EDGE_PWM);

    // 3. Pinos de Velocidade (PWM) - ENA e ENB
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 2); 
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 3); 

    // Ajusta velocidade para 50%
    pwm_tpm_CnV(TPM2, 0, duty_50);
    pwm_tpm_CnV(TPM2, 1, duty_50*1.8);

    while(1) {
        k_msleep(SLEEP_TIME_MS);
        // --- MOVER PARA FRENTE ---
        gpio_pin_set(port_c, M1_IN1, 1);
        gpio_pin_set(port_c, M1_IN2, 0);
        gpio_pin_set(port_c, M2_IN1, 1);
        gpio_pin_set(port_c, M2_IN2, 0);

        k_msleep(SLEEP_TIME_MS);

        // --- MOVER PARA TRÁS ---
        gpio_pin_set(port_c, M1_IN1, 0);
        gpio_pin_set(port_c, M1_IN2, 1);
        gpio_pin_set(port_c, M2_IN1, 0);
        gpio_pin_set(port_c, M2_IN2, 1);

        //k_msleep(SLEEP_TIME_MS);
    }
    
    return 0;
}
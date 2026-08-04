#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)

// Define o valor do registrador MOD do TPM para configurar o período do PWM
#define TPM_MODULE 4800         // Define a frequência do PWM fpwm = (TPM_CLK / (TPM_MODULE * PS))
// Valores de duty cycle correspondentes a diferentes larguras de pulso
uint16_t duty_50  = TPM_MODULE/2;       // 50% de duty cycle (meio brilho)

#define SLEEP_TIME_MS   2000

// --- Referências das portas GPIO para a direção ---
const struct device *port_c = DEVICE_DT_GET(DT_NODELABEL(gpioc));
const struct device *port_b = DEVICE_DT_GET(DT_NODELABEL(gpiob));

int main(void)
{
    gpio_pin_configure(port_c, 12, GPIO_OUTPUT_INACTIVE); 
    gpio_pin_configure(port_c, 13, GPIO_OUTPUT_INACTIVE);
    
    // Esquerda (IN3 e IN12) -> PTB2 e PTB3
    gpio_pin_configure(port_c, 16, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(port_c, 17, GPIO_OUTPUT_INACTIVE);
    // Inicializa o módulo TPM2 com:
    // - base do TPMx
    // - fonte de clock PLL/FLL (TPM_CLK)
    // - valor do registrador MOD
    // - tipo de clock (TPM_CLK)
    // - prescaler de 1 a 128 (PS)
    // - modo de operação EDGE_PWM
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_1, EDGE_PWM);

    // Inicializa o canal 0 do TPM2 para gerar sinal PWM na porta GPIOB_18
    // - modo TPM_PWM_H (nível alto durante o pulso)
    //pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 18);
    //pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOE, 22);
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 2); //direita+
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 3); //direita-

    // Define o valor do duty cycle: nesse caso, duty_100 (LED quase desligado)
    pwm_tpm_CnV(TPM2, 0, duty_50);
    pwm_tpm_CnV(TPM2, 1, duty_50);
    
        //pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 2); //esquerda+
        //pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_L, GPIOB, 3); //esquerda-
    

    while(1) {
        // --- MOVER PARA FRENTE ---
        gpio_pin_set(port_c, 12, 1);
        gpio_pin_set(port_c, 13, 0);
        gpio_pin_set(port_c, 16, 1);
        gpio_pin_set(port_c, 17, 0);

        k_msleep(SLEEP_TIME_MS);

        // --- MOVER PARA TRÁS ---
        gpio_pin_set(port_c, 12, 0);
        gpio_pin_set(port_c, 13, 1);
        gpio_pin_set(port_c, 16, 0);
        gpio_pin_set(port_c, 17, 1);

        k_msleep(SLEEP_TIME_MS);
    }
    
    return 0;
}
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* --- 1. Definições de Hardware via DeviceTree --- */
/* Estes aliases devem estar definidos no ficheiro frdm_kl25z.overlay */
static const struct gpio_dt_spec encoder_dir = GPIO_DT_SPEC_GET(DT_ALIAS(encdir), gpios);
static const struct gpio_dt_spec trigger_ultrassom = GPIO_DT_SPEC_GET(DT_ALIAS(trig), gpios);
static const struct pwm_dt_spec motor_esq_pwm = PWM_DT_SPEC_GET(DT_ALIAS(pwmesq));
/* Exemplo para o SPI do nRF24L01 (requer configuração completa no Devicetree) */
/* static const struct spi_dt_spec nrf_spi = SPI_DT_SPEC_GET(DT_NODELABEL(nrf24), spi_dev, ...); */

/* --- 2. Variáveis de Estado e Odometria --- */
typedef enum { ESTADO_STOP, ESTADO_RUN } estado_robo_t;
static estado_robo_t estado_atual = ESTADO_STOP;
static volatile uint32_t pulsos_totais = 0;

/* Estrutura para a interrupção do encoder */
static struct gpio_callback encoder_cb_data;

/* --- 3. Rotinas de Interrupção (ISR) --- */
void encoder_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    pulsos_totais++; /* Apenas soma, mesmo se recuar ou curvar */
}

/* --- 4. Configuração de Periféricos --- */
void inicializar_hardware(void) {
    /* Verificar se os dispositivos estão prontos no kernel */
    if (!gpio_is_ready_dt(&encoder_dir) || !pwm_is_ready_dt(&motor_esq_pwm)) {
        printk("Erro: Hardware não está pronto.\n");
        return;
    }

    /* Configurar Interrupção do Encoder (Borda Ativa) */
    gpio_pin_configure_dt(&encoder_dir, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&encoder_dir, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&encoder_cb_data, encoder_isr, BIT(encoder_dir.pin));
    gpio_add_callback(encoder_dir.port, &encoder_cb_data);

    /* Configurar Ultrassom */
    gpio_pin_configure_dt(&trigger_ultrassom, GPIO_OUTPUT_INACTIVE);
}

/* --- 5. Thread de Comunicação (nRF24L01+) --- */
#define STACK_SIZE 1024
#define PRIORITY_RADIO 7

void thread_radio_entry(void *p1, void *p2, void *p3) {
    while (1) {
        /* Pseudo-lógica de receção SPI do nRF24L01+ */
        char comando_recebido = 'N'; /* Substituir pela leitura SPI real */

        switch (comando_recebido) {
            case 'R': estado_atual = ESTADO_RUN; break;
            case 'S': estado_atual = ESTADO_STOP; break;
            case 'C': pulsos_totais = 0; break;
            case 'D': printk("Distancia: %d pulsos\n", pulsos_totais); break;
        }

        k_msleep(50); /* Liberta o CPU para a thread principal */
    }
}
K_THREAD_DEFINE(radio_tid, STACK_SIZE, thread_radio_entry, NULL, NULL, NULL, PRIORITY_RADIO, 0, 0);

/* --- 6. Thread Principal (Controlo e Navegação) --- */
int main(void) {
    printk("Iniciando Robô (Zephyr 4.2)...\n");
    inicializar_hardware();

    while (1) {
        if (estado_atual == ESTADO_RUN) {
            /* Lógica de leitura do ultrassom (Trigger e cálculo do Echo) */
            uint32_t distancia_cm = 20; /* Substituir pela leitura real do sensor */

            if (distancia_cm > 15) {
                /* Caminho livre: Avançar (Ex: Duty Cycle de 50%) */
                pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), PWM_MSEC(5));
            } else {
                /* Obstáculo: Inverter GPIOs de direção e curvar */
                pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), PWM_MSEC(3));
            }
        } else {
            /* Estado STOP: Motores a 0% */
            pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), 0);
        }

        k_msleep(20); /* Frequência da malha de controlo (50Hz) */
    }
    return 0;
}
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* --- Hardware via DeviceTree --- */
static const struct gpio_dt_spec encoder_dir = GPIO_DT_SPEC_GET(DT_ALIAS(encdir), gpios);
static const struct gpio_dt_spec trigger_ultrassom = GPIO_DT_SPEC_GET(DT_ALIAS(trig), gpios);
static const struct pwm_dt_spec motor_esq_pwm = PWM_DT_SPEC_GET(DT_ALIAS(pwmesq));

/* --- Variáveis do Sistema --- */
typedef enum { ESTADO_STOP, ESTADO_RUN } estado_robo_t;
static estado_robo_t estado_atual = ESTADO_STOP;
static volatile uint32_t pulsos_totais = 0;

static struct gpio_callback encoder_cb_data;

/* --- Interrupção do Encoder --- */
void encoder_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    pulsos_totais++;
}

/* --- Inicialização --- */
void inicializar_hardware(void) {
    if (!gpio_is_ready_dt(&encoder_dir) || !pwm_is_ready_dt(&motor_esq_pwm) || !gpio_is_ready_dt(&trigger_ultrassom)) {
        printk("Erro: Hardware nao esta pronto no kernel.\n");
        return;
    }

    gpio_pin_configure_dt(&encoder_dir, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&encoder_dir, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&encoder_cb_data, encoder_isr, BIT(encoder_dir.pin));
    gpio_add_callback(encoder_dir.port, &encoder_cb_data);

    gpio_pin_configure_dt(&trigger_ultrassom, GPIO_OUTPUT_INACTIVE);
}

/* --- Thread de Comunicação (nRF24L01+) --- */
#define STACK_SIZE 1024
#define PRIORITY_RADIO 7

void thread_radio_entry(void *p1, void *p2, void *p3) {
    while (1) {
        char comando_recebido = 'N'; // TODO: Implementar leitura SPI do rádio aqui

        switch (comando_recebido) {
            case 'R': estado_atual = ESTADO_RUN; break;
            case 'S': estado_atual = ESTADO_STOP; break;
            case 'C': pulsos_totais = 0; break;
            case 'D': printk("Distancia: %d pulsos\n", pulsos_totais); break;
        }

        k_msleep(50);
    }
}
K_THREAD_DEFINE(radio_tid, STACK_SIZE, thread_radio_entry, NULL, NULL, NULL, PRIORITY_RADIO, 0, 0);

/* --- Thread Principal (Navegação) --- */
int main(void) {
    printk("Iniciando Robô (Zephyr 4.2)...\n");
    inicializar_hardware();

    while (1) {
        if (estado_atual == ESTADO_RUN) {
            uint32_t distancia_cm = 20; // TODO: Implementar cálculo de tempo de eco do ultrassom

            if (distancia_cm > 15) {
                /* Caminho livre: PWM a 50% (5ms em período de 10ms) */
                pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), PWM_MSEC(5));
            } else {
                /* Desvio: PWM a 30% (3ms em período de 10ms) */
                pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), PWM_MSEC(3));
            }
        } else {
            /* STOP: Desliga motores */
            pwm_set_dt(&motor_esq_pwm, PWM_MSEC(10), 0);
        }

        k_msleep(20);
    }
    return 0;
}
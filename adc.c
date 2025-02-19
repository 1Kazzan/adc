#include <stdio.h>
#include "pico/stdlib.h" // biblioteca padrão para trabalhar com a raspberry pi pico
#include "hardware/adc.h" // biblioteca para trabalhar com adc
#include "hardware/pwm.h" // biblioteca para gerenciamento de temporizadores de hardware.
#include "inc/ssd1306.h"
#include "inc/font.h"

// display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
// joystick
#define joystick_X 26  // GPIO para eixo X
#define joystick_Y 27  // GPIO para eixo Y
#define joystick_button 22 // GPIO para botão do Joystick
// botões e led rgb
#define buttonA 5
#define led_red 13
#define led_blue 12
#define led_green 11

// variáveis para executar funções de interrupção e debouncing
volatile uint32_t last_time_A = 0;
volatile uint32_t last_time_joy = 0;

// Variáveis globais
bool estado_led_green = false; // desligado
bool estado_led_blue = false;  // desligado
int borda_estado = 1; // inicia no primeiro formato
bool controle_ativo = false; 

// Prototipação da função de interrupção
static void gpio_irq_handler(uint gpio, uint32_t events);

// Função de interrupção com debouncing
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica o debounce para o botão A
    if (gpio == buttonA && current_time - last_time_A > 200000) { // 200 ms de debounce
        last_time_A = current_time; // muda o último estado para atual    
        // Ação para o botão A
        if (!gpio_get(buttonA)) {
            controle_ativo = !controle_ativo;  // Alterna o estado de controle de luminosidade
        }
    }
    // Verifica o debounce para o joystick
    if (gpio == joystick_button && current_time - last_time_joy > 200000) {
        last_time_joy = current_time; // muda o último estado para atual        
        // Ação para o botão do joystick
        if (!gpio_get(joystick_button)) {
            estado_led_green = !estado_led_green; // Alterna o estado do led verde
            gpio_put(led_green, estado_led_green); // Atualiza o led

            if (borda_estado == 1) {
                borda_estado = 2;  // Se for um retângulos, muda para dois retângulos
            } else if (borda_estado == 2) { 
                borda_estado = 1;  // Se for dois retângulos, volta para um retângulo
            }
        }
    }
}

int main() {
    stdio_init_all(); // permite uso de printf

    // eixos joystick
    adc_init();
    adc_gpio_init(joystick_X);
    adc_gpio_init(joystick_Y); 

    // iniciar led
    gpio_init(led_green);
    gpio_set_dir(led_green, GPIO_OUT);
    gpio_put(led_green, false); 

    // Inicia botões
    gpio_init(buttonA);
    gpio_set_dir(buttonA, GPIO_IN);
    gpio_pull_up(buttonA);

    gpio_init(joystick_button);
    gpio_set_dir(joystick_button, GPIO_IN);
    gpio_pull_up(joystick_button); 

    // Inicia o I2C com 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    // Defini os pinos GPIO I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); 
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
    gpio_pull_up(I2C_SDA); 
    gpio_pull_up(I2C_SCL); 
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa os LEDs com PWM
    gpio_set_function(led_red, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice_red = pwm_gpio_to_slice_num(led_red);  //obter o canal PWM da GPIO
    pwm_set_wrap(slice_red, 4096); // Define o valor de wrap
    pwm_set_enabled(slice_red, true);

    gpio_set_function(led_blue, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice_blue = pwm_gpio_to_slice_num(led_blue);  //obter o canal PWM da GPIO
    pwm_set_wrap(slice_blue, 4096); // Define o valor de wrap
    pwm_set_enabled(slice_blue, true);

    // Aplicando interrupções e suas funcionalidades
    gpio_set_irq_enabled_with_callback(buttonA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(joystick_button, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true) {
        if (controle_ativo) {
            adc_select_input(0);
            uint16_t valor_x = adc_read();
            adc_select_input(1);
            uint16_t valor_y = adc_read();

            // Verificar se o joystick está no centro (desligar LEDs nesse caso)
            if (valor_x >= 1800 && valor_x <= 2200 && valor_y >= 1800 && valor_y <= 2200) {
                pwm_set_gpio_level(led_red, 0);
                pwm_set_gpio_level(led_blue, 0);
            } else {
                pwm_set_gpio_level(led_red, valor_x);
                pwm_set_gpio_level(led_blue, valor_y);
            }
        }
        else {
            pwm_set_gpio_level(led_red, 0);
            pwm_set_gpio_level(led_blue, 0);
        }

        ssd1306_fill(&ssd, false);  // Limpa o display
// Atualiza a borda do display OLED com base no estado
if (borda_estado == 1) {
    // um retangulo
    ssd1306_rect(&ssd, 3, 3, 122, 58, 1, 0);
} else if (borda_estado == 2) {
    // dois retangulos 
    ssd1306_rect(&ssd, 3, 3, 122, 58, 1, 0);
    ssd1306_rect(&ssd, 1, 1, 126, 62, 1, 0); 
    }
    ssd1306_send_data(&ssd); // Envia os dados para o display
}
}
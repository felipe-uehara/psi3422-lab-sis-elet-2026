#ifndef MMA8451Q_H
#define MMA8451Q_H

#include "mbed.h"

/**
* Biblioteca para controle do acelerômetro digital MMA8451Q integrado na FRDM-KL25Z.
*/
class MMA8451Q
{
public:
    /**
    * Construtor da classe MMA8451Q.
    *
    * @param sda Pino de dados I2C (Normalmente PTE25 na KL25Z)
    * @param scl Pino de clock I2C (Normalmente PTE24 na KL25Z)
    * @param addr Endereço I2C do sensor (Default: 0x1D << 1)
    */
    MMA8451Q(PinName sda, PinName scl, int addr);

    /**
    * Destrutor da classe.
    */
    ~MMA8451Q();

    /**
    * Obtém o identificador do dispositivo (Who Am I).
    *
    * @returns O valor do registrador WHO_AM_I (Deve retornar 0x1A)
    */
    uint8_t getWhoAmI();

    /**
    * Lê a aceleração no eixo X.
    *
    * @returns Valor em Gs do eixo X
    */
    float getAccX();

    /**
    * Lê a aceleração no eixo Y.
    *
    * @returns Valor em Gs do eixo Y
    */
    float getAccY();

    /**
    * Lê a aceleração no eixo Z.
    *
    * @returns Valor em Gs do eixo Z
    */
    float getAccZ();

    /**
    * Lê os valores dos três eixos simultaneamente de forma otimizada.
    *
    * @param res Ponteiro para um array de float com tamanho mínimo de 3 posições
    */
    void getAccAllAxis(float * res);

private:
    I2C m_i2c;
    int m_addr;
    
    // Funções auxiliares para leitura e escrita de registradores via I2C
    void readRegs(int addr, uint8_t * data, int len);
    void writeRegs(uint8_t * data, int len);
    int16_t getAccAxis(uint8_t addr);
};

#endif

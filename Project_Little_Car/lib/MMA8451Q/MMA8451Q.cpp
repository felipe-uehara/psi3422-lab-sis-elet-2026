#include "MMA8451Q.h"

// Registradores internos do MMA8451Q
#define REG_WHO_AM_I      0x0D
#define REG_CTRL_REG1     0x2A
#define REG_OUT_X_MSB     0x01
#define REG_OUT_Y_MSB     0x03
#define REG_OUT_Z_MSB     0x05

// Fator de escala padrão para o modo de 2g (Resolução de 14 bits)
#define UINT14_MAX        16383

MMA8451Q::MMA8451Q(PinName sda, PinName scl, int addr) : m_i2c(sda, scl), m_addr(addr) {
    // Configura e ativa o sensor (coloca em modo Active com ODR de 800Hz)
    uint8_t data[2] = {REG_CTRL_REG1, 0x01};
    writeRegs(data, 2);
}

MMA8451Q::~MMA8451Q() { }

uint8_t MMA8451Q::getWhoAmI() {
    uint8_t res = 0;
    readRegs(REG_WHO_AM_I, &res, 1);
    return res;
}

float MMA8451Q::getAccX() {
    return (float(getAccAxis(REG_OUT_X_MSB)) / 4096.0f);
}

float MMA8451Q::getAccY() {
    return (float(getAccAxis(REG_OUT_Y_MSB)) / 4096.0f);
}

float MMA8451Q::getAccZ() {
    return (float(getAccAxis(REG_OUT_Z_MSB)) / 4096.0f);
}

void MMA8451Q::getAccAllAxis(float * res) {
    res[0] = getAccX();
    res[1] = getAccY();
    res[2] = getAccZ();
}

int16_t MMA8451Q::getAccAxis(uint8_t addr) {
    int16_t acc;
    uint8_t res[2];
    
    // O sensor possui dados de 14 bits justificados à esquerda em 2 registradores de 8 bits
    readRegs(addr, res, 2);
    acc = (res[0] << 6) | (res[1] >> 2);
    
    // Tratamento para números negativos (Complemento de 2 em 14 bits)
    if (acc > 8191) {
        acc -= 16384;
    }
    
    return acc;
}

void MMA8451Q::readRegs(int addr, uint8_t * data, int len) {
    char t[1] = {static_cast<char>(addr)};
    m_i2c.write(m_addr, t, 1, true); // Envia o endereço do registrador (com repeated-start)
    m_i2c.read(m_addr, (char *)data, len); // Lê os dados retornados
}

void MMA8451Q::writeRegs(uint8_t * data, int len) {
    m_i2c.write(m_addr, (char *)data, len);
}

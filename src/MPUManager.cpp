#include <Wire.h>
#include <math.h> 
#include "MPUManager.h"
#include "MPUData.h"

// ================= SABİT DEĞERLER (CONSTANTS) =================
const uint8_t MPU_ADDR = 0x68;          // MPU6050 Varsayılan I2C Adresi
const uint8_t REG_PWR_MGMT_1 = 0x6B;    // Güç Yönetimi Yazmacı (Register)
const uint8_t REG_ACCEL_START = 0x3B;   // İvmeölçer Veri Başlangıç Yazmacı
const uint8_t MPU_RESET_CMD = 0x80;     // Cihazı Sıfırlama (Reset) Komutu
const float RAD_TO_DEG = 57.2957795f;   // Radyanı Dereceye Çevirme Katsayısı

// ================= FONKSİYON PROTOTİPİ =================
// 'isReset' parametresi sonda varsayılan olarak false tanımlandı
MPUData calcAngles(
    float gyro_sens, float smooth, float dt, int16_t AcX, int16_t AcY,
    int16_t AcZ, int16_t GyX, int16_t GyY, int16_t GyZ, bool isReset = false
);

// ================= SINIF FONKSİYONLARI =================

bool MPUManager::begin() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_PWR_MGMT_1);
    Wire.write(0); // Sensörü uyandır
    Wire.endTransmission(true);
    return true;
}

MPUData MPUManager::readMPU(float gyro_sens, float smooth, float dt) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_ACCEL_START);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, (uint8_t)14, true);

    int16_t AcX = Wire.read() << 8 | Wire.read();
    int16_t AcY = Wire.read() << 8 | Wire.read();
    int16_t AcZ = Wire.read() << 8 | Wire.read();
    Wire.read(); Wire.read(); // Sıcaklık verisini atla
    int16_t GyX = Wire.read() << 8 | Wire.read();
    int16_t GyY = Wire.read() << 8 | Wire.read();
    int16_t GyZ = Wire.read() << 8 | Wire.read();

    return calcAngles(gyro_sens, smooth, dt, AcX, AcY, AcZ, GyX, GyY, GyZ, false);
}

void MPUManager::reset() {
    // Sensöre donanımsal reset sinyali gönderiliyor
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(REG_PWR_MGMT_1);
    Wire.write(MPU_RESET_CMD);
    Wire.endTransmission(true);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Sensör sıfırlandıktan sonra tekrar başlatılıyor (Uyandırılıyor)
    this->begin();

    // calcAngles içindeki static tutulan açı hafızası temizleniyor
    calcAngles(0, 0, 0, 0, 0, 0, 0, 0, 0, true);
}

// ================= MATEMATİKSEL HESAPLAMA MOTORU =================

MPUData calcAngles(
    float gyro_sens, float smooth, float dt, int16_t AcX, int16_t AcY,
    int16_t AcZ, int16_t GyX, int16_t GyY, int16_t GyZ, bool isReset
) {
    static float savedRoll = 0.0f;
    static float savedPitch = 0.0f;
    static float savedYaw = 0.0f;

    // Reset tetiklendiyse hafızayı temizle ve boş veri dön
    if (isReset) {
        savedRoll = 0.0f;
        savedPitch = 0.0f;
        savedYaw = 0.0f;
        return MPUData();
    }

    // İvmeölçer ham açı hesaplamaları (Sabit katsayı kullanıldı)
    float accelPitch = -(atan2(AcY, AcZ) * RAD_TO_DEG);
    float accelRoll = atan2(AcX, sqrt(AcY * AcY + AcZ * AcZ)) * RAD_TO_DEG;

    // Tamamlayıcı Filtre (Complementary Filter) aşaması
    savedRoll += smooth * (accelRoll - savedRoll);
    savedPitch += smooth * (accelPitch - savedPitch);

    // Jiroskop kümülatif entegrasyonu (Yaw hesabı)
    float gz = GyZ / gyro_sens;
    savedYaw += gz * dt;

    // Verilerin paketlenmesi
    MPUData mpuData;
    mpuData.roll = savedRoll;
    mpuData.pitch = savedPitch;
    mpuData.yaw = savedYaw;

    return mpuData;
}
#include <iostream>
#include <fstream>
#include <string>
#include <thread> // 用于延时
#include <chrono>

// LED 路径类封装
class LED {
private:
    std::string path;
public:
    LED(const std::string& ledPath) : path(ledPath) {}

    // 设置亮度 (1=亮, 0=灭)
    bool setBrightness(int value) {
        std::ofstream fs(path + "/brightness");
        if (!fs.is_open()) {
            std::cerr << "Failed to open LED brightness file!" << std::endl;
            return false;
        }
        fs << value;
        fs.close();
        return true;
    }

    // 关闭触发器 (防止干扰)
    bool disableTrigger() {
        std::ofstream fs(path + "/trigger");
        if (!fs.is_open()) return false;
        fs << "none";
        fs.close();
        return true;
    }
};

int main() {
    // 实例化 LED 对象
    LED myLed("/sys/class/leds/user-led0");

    std::cout << "Starting C++ LED Blink..." << std::endl;

    // 先关掉触发器
    myLed.disableTrigger();

    while (true) {
        // 亮
        myLed.setBrightness(1);
        std::cout << "ON" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 灭
        myLed.setBrightness(0);
        std::cout << "OFF" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
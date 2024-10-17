#include <iostream>
#include <cstring>
#include <string>
#include <locale>
#include <cstdint> // Добавлено для uint32_t

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") // Для MSVC
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

int recv_all(int socket, char* buffer, int size) {
    int total_received = 0;
    while (total_received < size) {
        int received = recv(socket, buffer + total_received, size - total_received, 0);
        if (received <= 0) {
            return -1;
        }
        total_received += received;
    }
    return total_received;
}

int main() {
    std::setlocale(LC_ALL, "Russian");

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cout << "Ошибка инициализации Winsock." << std::endl;
        return 1;
    }
#endif

    int sock;
    struct sockaddr_in server;

    // Создание сокета
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "Не удалось создать сокет." << std::endl;
        return 1;
    }

    // Настройка адреса сервера
    server.sin_addr.s_addr = inet_addr("192.168.100.103"); // Замените на IP-адрес сервера
    server.sin_family = AF_INET;
    server.sin_port = htons(8080); // Порт сервера

    // Подключение к серверу
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cout << "Ошибка подключения к серверу." << std::endl;
        return 1;
    }

    std::cout << "Подключено к серверу." << std::endl;

    // Основной цикл работы клиента
    while (true) {
        // Получение сообщения от пользователя
        std::string user_input;
        std::cout << "Введите сообщение (или 'exit' для выхода): ";
        std::getline(std::cin, user_input);

        // Проверка на команду выхода
        if (user_input == "exit") {
            break;
        }

        // Отправка данных
        const char *message = user_input.c_str();
        int message_length = user_input.length();

        // Отправляем размер сообщения
        uint32_t net_message_length = htonl(message_length); // Приводим к сетевому порядку байт
        if (send(sock, (char*)&net_message_length, sizeof(net_message_length), 0) <= 0) {
            std::cout << "Ошибка при отправке данных на сервер." << std::endl;
            break;
        }

        // Отправляем само сообщение
        if (send(sock, message, message_length, 0) <= 0) {
            std::cout << "Ошибка при отправке данных на сервер." << std::endl;
            break;
        }

        // Получение ответа от сервера
        uint32_t net_recv_size;
        if (recv_all(sock, (char*)&net_recv_size, sizeof(net_recv_size)) <= 0) {
            std::cout << "Ошибка при получении размера ответа от сервера." << std::endl;
            break;
        }
        int recv_size = ntohl(net_recv_size);

        char *server_reply = new char[recv_size + 1];
        if (recv_all(sock, server_reply, recv_size) <= 0) {
            std::cout << "Ошибка при получении ответа от сервера." << std::endl;
            delete[] server_reply;
            break;
        }
        server_reply[recv_size] = '\0';

        std::cout << "Ответ от сервера: " << server_reply << std::endl;

        delete[] server_reply;
    }

    // Закрытие сокета и очистка ресурсов
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    std::cout << "Клиент завершил работу." << std::endl;
    return 0;
}

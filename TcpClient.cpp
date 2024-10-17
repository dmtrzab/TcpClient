// TcpClient.cpp
#include <iostream>
#include <cstring>
#include <string>
#include <locale>
#include <cstdint> // Для uint32_t
#include <netdb.h> // Для gethostbyname
#include <arpa/inet.h> // Для inet_pton
#include <unistd.h> // Для close()

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") // Для MSVC
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

const size_t MAX_MESSAGE_SIZE = 1024 * 1024;

// Функция для гарантированного чтения N байт из сокета
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

// Функция для разрешения доменного имени в IP
bool resolveHostname(const std::string& hostname, struct sockaddr_in& server_addr) {
    struct hostent* he;
    struct in_addr** addr_list;
    
    if ((he = gethostbyname(hostname.c_str())) == NULL) {
        // Не удалось разрешить имя
        return false;
    }
    
    addr_list = (struct in_addr**) he->h_addr_list;
    
    if (addr_list[0] != NULL) {
        server_addr.sin_addr = *addr_list[0];
        return true;
    }
    
    return false;
}

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "Russian");

    // Проверка аргументов командной строки
    if (argc != 3) {
        std::cout << "Использование: " << argv[0] << " <IP или домен сервера> <порт>" << std::endl;
        return 1;
    }

    std::string server_input = argv[1];
    uint16_t server_port = std::stoi(argv[2]);

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

    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);

    // Проверка, является ли ввод IP-адресом или доменным именем
    if (inet_pton(AF_INET, server_input.c_str(), &server.sin_addr) <= 0) {
        // Не удалось преобразовать как IP, попробуем как доменное имя
        if (!resolveHostname(server_input, server)) {
            std::cout << "Не удалось разрешить доменное имя сервера." << std::endl;
#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            return 1;
        }
    }

    // Подключение к серверу
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        std::cout << "Ошибка подключения к серверу." << std::endl;
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
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
        uint32_t message_length = static_cast<uint32_t>(user_input.length());

        // Отправляем размер сообщения
        uint32_t net_message_length = htonl(message_length); // Приводим к сетевому порядку байт
        if (send(sock, reinterpret_cast<char*>(&net_message_length), sizeof(net_message_length), 0) <= 0) {
            std::cout << "Ошибка при отправке размера сообщения на сервер." << std::endl;
            break;
        }

        // Отправляем само сообщение
        if (send(sock, message, message_length, 0) <= 0) {
            std::cout << "Ошибка при отправке данных на сервер." << std::endl;
            break;
        }

        // Получение ответа от сервера
        uint32_t net_recv_size;
        if (recv_all(sock, reinterpret_cast<char*>(&net_recv_size), sizeof(net_recv_size)) <= 0) {
            std::cout << "Ошибка при получении размера ответа от сервера." << std::endl;
            break;
        }
        uint32_t recv_size = ntohl(net_recv_size);

        if (recv_size == 0 || recv_size > MAX_MESSAGE_SIZE) {
            std::cout << "Некорректный размер ответа от сервера." << std::endl;
            break;
        }

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

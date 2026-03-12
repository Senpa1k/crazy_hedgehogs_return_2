#include "crow.h"
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <regex>
#include <cctype>

using namespace std;
using namespace crow;

// === Валидация ===

bool isValidName(const string& name) {
    return !name.empty() && name.find_first_not_of(" \t\n\r") != string::npos;
}

bool isValidPhone(const string& phone) {
    if (phone.empty()) return false;
    
    int digits = 0;
    for (char c : phone) {
        if (isdigit(c)) digits++;
        else if (c != '+' && c != '-' && c != ' ' && c != '(' && c != ')') 
            return false;
    }
    return digits >= 10;
}

bool isValidNote(const string& note) {
    return note.size() <= 500;
}

// Хелпер для ошибок в JSON
crow::response jsonError(int code, const string& message) {
    crow::json::wvalue err;
    err["error"] = message;
    crow::response res(code, err);
    res.add_header("Content-Type", "application/json");
    return res;
}

// Разделение ФИО
void splitName(const string& fullName, string& lastName, string& firstName, string& patronymic) {
    istringstream iss(fullName);
    iss >> lastName;
    if (!(iss >> firstName)) firstName = "-";
    string temp;
    while (iss >> temp) {
        patronymic += (patronymic.empty() ? "" : " ") + temp;
    }
}

int main() {
    SimpleApp app;
    string conn_string = "dbname=phone_book user=postgres password=postgres host=postgres port=5432";

    // Главная страница
    CROW_ROUTE(app, "/")([]() {
        crow::response res;
        res.set_static_file_info("public/index.html");
        return res;
    });

    // Картинка
    CROW_ROUTE(app, "/phone.jpg")([]() {
        crow::response res;
        res.set_static_file_info("public/phone.jpg");
        return res;
    });

    // === GET /api/contacts ===
    CROW_ROUTE(app, "/api/contacts").methods(HTTPMethod::GET)([&conn_string]() {
        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            pqxx::result r = w.exec("SELECT id, last_name, first_name, patronymic, phone_number, note FROM contacts ORDER BY id");
            
            vector<json::wvalue> items;
            for (auto row : r) {
                string fio = row[1].c_str();
                fio += " " + string(row[2].c_str());
                if (!row[3].is_null()) fio += " " + string(row[3].c_str());

                json::wvalue item;
                item["id"] = row[0].as<int>();
                item["name"] = fio;
                item["phone"] = row[4].c_str();
                item["note"] = row[5].is_null() ? "" : row[5].c_str();
                items.push_back(item);
            }
            
            crow::json::wvalue response(items);
            crow::response res(response);
            res.add_header("Content-Type", "application/json");
            return res;
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
    });

    // === POST /api/contacts ===
    CROW_ROUTE(app, "/api/contacts").methods(HTTPMethod::POST)([&conn_string](const request& req) {
        auto body = json::load(req.body);
        if (!body) return jsonError(400, "Invalid JSON format");

        if (!body.has("name") || !body.has("phone")) 
            return jsonError(400, "Missing required fields: name, phone");
        
        // ✅ Конвертация r_string -> std::string
        string name = std::string(body["name"].s());
        string phone = std::string(body["phone"].s());
        
        // ✅ Безопасное получение опционального поля note
        string note;
        if (body.has("note") && body["note"].t() != crow::json::type::Null) {
            note = std::string(body["note"].s());
        } else {
            note = "";
        }

        if (!isValidName(name)) 
            return jsonError(400, "Invalid name: cannot be empty");
        if (!isValidPhone(phone)) 
            return jsonError(400, "Invalid phone: must contain at least 10 digits and only +,-,(),spaces");
        if (!isValidNote(note)) 
            return jsonError(400, "Invalid note: maximum 500 characters allowed");

        string lastName, firstName, patronymic;
        splitName(name, lastName, firstName, patronymic);

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            w.exec_params("INSERT INTO contacts (last_name, first_name, patronymic, phone_number, note) VALUES ($1, $2, $3, $4, $5)",
                          lastName, firstName, patronymic, phone, note);
            w.commit();
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
        return crow::response(201);
    });

    // === PUT /api/contacts/<int> ===
    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::PUT)([&conn_string](const request& req, int id) {
        if (id <= 0) return jsonError(400, "Invalid contact ID");

        auto body = json::load(req.body);
        if (!body) return jsonError(400, "Invalid JSON format");

        if (!body.has("name") || !body.has("phone")) 
            return jsonError(400, "Missing required fields: name, phone");
        
        // ✅ Конвертация r_string -> std::string (без .str())
        string name = std::string(body["name"].s());
        string phone = std::string(body["phone"].s());
        
        string note;
        if (body.has("note") && body["note"].t() != crow::json::type::Null) {
            note = std::string(body["note"].s());
        } else {
            note = "";
        }

        if (!isValidName(name)) 
            return jsonError(400, "Invalid name: cannot be empty");
        if (!isValidPhone(phone)) 
            return jsonError(400, "Invalid phone: must contain at least 10 digits");
        if (!isValidNote(note)) 
            return jsonError(400, "Invalid note: maximum 500 characters allowed");

        string lastName, firstName, patronymic;
        splitName(name, lastName, firstName, patronymic);

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            
            // ✅ Сохраняем результат для affected_rows()
            auto result = w.exec_params(
                "UPDATE contacts SET last_name=$1, first_name=$2, patronymic=$3, phone_number=$4, note=$5 WHERE id=$6",
                lastName, firstName, patronymic, phone, note, id);
            w.commit();
            
            // ✅ Проверяем через result.affected_rows()
            if (result.affected_rows() == 0)
                return jsonError(404, "Contact not found");
                
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
        return crow::response(200);
    });

    // === DELETE /api/contacts/<int> ===
    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::DELETE)([&conn_string](int id) {
        if (id <= 0) return jsonError(400, "Invalid contact ID");

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            
            // ✅ Сохраняем результат для affected_rows() (опционально)
            auto result = w.exec_params("DELETE FROM contacts WHERE id=$1", id);
            w.commit();
            
            // Если нужно возвращать 404 при удалении несуществующего:
            // if (result.affected_rows() == 0)
            //     return jsonError(404, "Contact not found");
            
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
        return crow::response(200);
    });

    // ✅ Запуск в однопоточном режиме
    app.port(8080).run(); 
}
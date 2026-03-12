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

bool isValidName(const string& name) {
    return !name.empty() && name.find_first_not_of(" \t\n\r") != string::npos;
}

bool isValidPhone(const string& phone) {
    if (phone.empty()) return false;
    int digits = 0;
    for (unsigned char c : phone) {
        if (std::isdigit(c)) {
            digits++;
        }
        else if (c != '+' && c != '-' && c != ' ' && c != '(' && c != ')') {
            return false;
        }
    }
    return digits >= 10;
}

bool isValidNote(const string& note) {
    return note.size() <= 500;
}

crow::response jsonError(int code, const string& message) {
    crow::json::wvalue err;
    err["error"] = message;
    crow::response res(code, err);
    res.add_header("Content-Type", "application/json");
    return res;
}

void splitName(const string& fullName, string& lastName, string& firstName, string& patronymic) {
    istringstream iss(fullName);
    iss >> lastName;
    if (!(iss >> firstName)) firstName = "-";
    string temp;
    while (iss >> temp) {
        patronymic += (patronymic.empty() ? "" : " ") + temp;
    }
}

bool getStringField(const crow::json::rvalue& json, const string& key, string& out) {
    if (!json.has(key)) return false;
    if (json[key].t() != crow::json::type::String) return false;
    out = json[key].s();
    return true;
}

int main() {
    SimpleApp app;
    string conn_string = "dbname=phone_book user=postgres password=postgres host=postgres port=5432";

    CROW_ROUTE(app, "/")([]() {
        crow::response res;
        res.set_static_file_info("public/index.html");
        return res;
    });

    CROW_ROUTE(app, "/phone.jpg")([]() {
        crow::response res;
        res.set_static_file_info("public/phone.jpg");
        return res;
    });

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

    CROW_ROUTE(app, "/api/contacts").methods(HTTPMethod::POST)([&conn_string](const request& req) {
        auto body = json::load(req.body);
        if (!body) return jsonError(400, "Invalid JSON format");

        if (!body.has("name") || !body.has("phone")) 
            return jsonError(400, "Missing required fields: name, phone");
        
        string name, phone, note;
        if (!getStringField(body, "name", name)) 
            return jsonError(400, "Invalid name field");
        if (!getStringField(body, "phone", phone)) 
            return jsonError(400, "Invalid phone field");
        if (body.has("note") && body["note"].t() != crow::json::type::Null) {
            note = body["note"].s();
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

    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::PUT)([&conn_string](const request& req, int id) {
        if (id <= 0) return jsonError(400, "Invalid contact ID");

        auto body = json::load(req.body);
        if (!body) return jsonError(400, "Invalid JSON format");

        if (!body.has("name") || !body.has("phone")) 
            return jsonError(400, "Missing required fields: name, phone");
        
        string name, phone, note;
        if (!getStringField(body, "name", name)) 
            return jsonError(400, "Invalid name field");
        if (!getStringField(body, "phone", phone)) 
            return jsonError(400, "Invalid phone field");
        if (body.has("note") && body["note"].t() != crow::json::type::Null) {
            note = body["note"].s();
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
            auto result = w.exec_params(
                "UPDATE contacts SET last_name=$1, first_name=$2, patronymic=$3, phone_number=$4, note=$5 WHERE id=$6",
                lastName, firstName, patronymic, phone, note, id);
            w.commit();
            
            if (result.affected_rows() == 0)
                return jsonError(404, "Contact not found");
                
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
        return crow::response(200);
    });

    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::DELETE)([&conn_string](int id) {
        if (id <= 0) return jsonError(400, "Invalid contact ID");

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            auto result = w.exec_params("DELETE FROM contacts WHERE id=$1", id);
            w.commit();
        } catch (const exception &e) {
            return jsonError(500, "Database error: " + string(e.what()));
        }
        return crow::response(200);
    });

    app.port(8080).run(); 
}
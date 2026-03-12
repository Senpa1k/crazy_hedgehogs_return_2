#include "crow.h"
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

using namespace std;
using namespace crow;

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
            
            json::wvalue response;
            response = static_cast<json::wvalue>(items);
            
            crow::response res(response);
            res.add_header("Content-Type", "application/json");
            return res;
        } catch (const exception &e) {
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/api/contacts").methods(HTTPMethod::POST)([&conn_string](const request& req) {
        auto body = json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        string lastName, firstName, patronymic;
        splitName(body["name"].s(), lastName, firstName, patronymic);

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            string phone = body["phone"].s();
            string note = body["note"].s();
            w.exec_params("INSERT INTO contacts (last_name, first_name, patronymic, phone_number, note) VALUES ($1, $2, $3, $4, $5)",
                          lastName, firstName, patronymic, phone, note);
            w.commit();
        } catch (const exception &e) {
            return crow::response(500, e.what());
        }
        return crow::response(201);
    });

    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::PUT)([&conn_string](const request& req, int id) {
        auto body = json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        string lastName, firstName, patronymic;
        splitName(body["name"].s(), lastName, firstName, patronymic);

        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            string phone = body["phone"].s();
            string note = body["note"].s();
            w.exec_params("UPDATE contacts SET last_name=$1, first_name=$2, patronymic=$3, phone_number=$4, note=$5 WHERE id=$6",
                          lastName, firstName, patronymic, phone, note, id);
            w.commit();
        } catch (const exception &e) {
            return crow::response(500, e.what());
        }
        return crow::response(200);
    });

    CROW_ROUTE(app, "/api/contacts/<int>").methods(HTTPMethod::DELETE)([&conn_string](int id) {
        try {
            pqxx::connection c(conn_string);
            pqxx::work w(c);
            w.exec_params("DELETE FROM contacts WHERE id=$1", id);
            w.commit();
        } catch (const exception &e) {
            return crow::response(500, e.what());
        }
        return crow::response(200);
    });

    app.port(8080).multithreaded().run();
}
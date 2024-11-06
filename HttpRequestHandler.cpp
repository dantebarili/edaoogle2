/**
 * @file HttpRequestHandler.h
 * @author Marc S. Ressl
 * @brief EDAoggle search engine
 * @version 0.3
 *
 * @copyright Copyright (c) 2022-2024 Marc S. Ressl
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <vector>

#include "HttpRequestHandler.h"

using namespace std;

HttpRequestHandler::HttpRequestHandler(string homePath)
{
    this->homePath = homePath;
}

/**
 * @brief Serves a webpage from file
 *
 * @param url The URL
 * @param response The HTTP response
 * @return true URL valid
 * @return false URL invalid
 */
bool HttpRequestHandler::serve(string url, vector<char> &response)
{
    // Blocks directory traversal
    // e.g. https://www.example.com/show_file.php?file=../../MyFile
    // * Builds absolute local path from url
    // * Checks if absolute local path is within home path
    auto homeAbsolutePath = filesystem::absolute(homePath);
    auto relativePath = homeAbsolutePath / url.substr(1);
    string path = filesystem::absolute(relativePath.make_preferred()).string();

    if (path.substr(0, homeAbsolutePath.string().size()) != homeAbsolutePath)
        return false;

    // Serves file
    ifstream file(path);
    if (file.fail())
        return false;

    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, ios::beg);

    response.resize(fileSize);
    file.read(response.data(), fileSize);

    return true;
}

bool createFTSTable(sqlite3* db) {
    const char* createTableQuery = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS keyword_index_fts USING fts5(keyword, URL, frequency);
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, createTableQuery, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            cerr << "Error al crear la tabla FTS: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        cout << "Tabla FTS creada correctamente." << endl;
        return true;
    }
    else {
        cerr << "Error al crear la tabla FTS: " << sqlite3_errmsg(db) << endl;
        return false;
    }
}

bool insertDataIntoFTS(sqlite3* db) {
    const char* insertDataQuery = R"(
        INSERT INTO keyword_index_fts (keyword, URL, frequency)
        SELECT keyword, url, frequency FROM keyword_index;
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insertDataQuery, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            cerr << "Error al insertar los datos en la tabla FTS: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        cout << "Datos insertados en la tabla FTS correctamente." << endl;
        return true;
    }
    else {
        cerr << "Error al preparar la consulta de insercion: " << sqlite3_errmsg(db) << endl;
        return false;
    }
}

bool searchUsingFTS(sqlite3* db, const string& searchString, vector<string>& results) {
    const char* query = R"(
        SELECT URL, SUM(frequency) AS total_frequency
        FROM keyword_index_fts
        WHERE keyword MATCH ?
        GROUP BY URL
        ORDER BY total_frequency DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, searchString.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* pageName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

            if (pageName) {
                results.push_back(pageName); 
            }
        }

        sqlite3_finalize(stmt);
        return true;
    }
    else {
        cerr << "Error al buscar con FTS: " << sqlite3_errmsg(db) << endl;
        return false;
    }
}


bool HttpRequestHandler::handleRequest(string url,
    HttpArguments arguments,
    vector<char>& response)
{
    string searchPage = "/search";
    if (url.substr(0, searchPage.size()) == searchPage)
    {
        string searchString;
        if (arguments.find("q") != arguments.end())
            searchString = arguments["q"];

        // Conectar a la base de datos SQLite
        sqlite3* db;
        if (sqlite3_open("C:/Users/dante/OneDrive/Documentos/git/edaoogle2/search_index.db", &db) != SQLITE_OK)
        {
            cerr << "Error al abrir la base de datos" << endl;
            return false;
        }

        // Vector para almacenar los nombres de las páginas en el orden deseado
        vector<string> results;

        // Crear tabla FTS e insterarle los datos
        if (!createFTSTable(db)) {      
            return 1;
        }

        if(!insertDataIntoFTS(db)){            
            return 1;
        }

        if(!searchUsingFTS(db, searchString, results)){            
            return 1;
        }

        sqlite3_close(db);

        // Construcción del HTML con los resultados
        string responseString = "<!DOCTYPE html>\
<html>\
<head>\
    <meta charset=\"utf-8\" />\
    <title>EDAoogle</title>\
    <link rel=\"preload\" href=\"https://fonts.googleapis.com\" />\
    <link rel=\"preload\" href=\"https://fonts.gstatic.com\" crossorigin />\
    <link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;800&display=swap\" rel=\"stylesheet\" />\
    <link rel=\"preload\" href=\"../css/style.css\" />\
    <link rel=\"stylesheet\" href=\"../css/style.css\" />\
</head>\
<body>\
    <article class=\"edaoogle\">\
        <div class=\"title\"><a href=\"/\">EDAoogle</a></div>\
        <div class=\"search\">\
            <form action=\"/search\" method=\"get\">\
                <input type=\"text\" name=\"q\" value=\"" +
            searchString + "\" autofocus>\
            </form>\
        </div>";

        // Muestra los resultados de la búsqueda en el HTML
        float searchTime = 0.1F; // Simula el tiempo de búsqueda
        responseString += "<div class=\"results\">" + to_string(results.size()) +
            " results (" + to_string(searchTime) + " seconds):</div>";
        for (auto& result : results)
            responseString += "<div class=\"result\"><a href=\"#\">" + result + "</a></div>";

        // Cierra el HTML
        responseString += "</article>\
</body>\
</html>";

        // Convierte `responseString` a `vector<char>` para `response`
        response.assign(responseString.begin(), responseString.end());

        return true;
    }
    else
    {
        return serve(url, response); // Sirve archivo estático si no es búsqueda
    }

    return false;
}

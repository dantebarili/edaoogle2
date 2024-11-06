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

#include "HttpRequestHandler.h"

using namespace std;

struct SearchResult {
    string pageName;
    int totalFrequency;
};

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

bool insertDataIntoFTS(sqlite3* db) {
    const char* insertDataQuery = R"(
        INSERT INTO keyword_index_fts (keyword, URL, frequency)
        SELECT keyword, URL, frequency FROM keyword_index;
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
        return false;
    }
}

bool createFTSTable(sqlite3* db) {
    const char* createTableQuery = R"(
        CREATE VIRTUAL TABLE keyword_index_fts USING fts5(keyword, URL, frequency);
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
        return false;
    }
}

bool searchUsingFTS(sqlite3* db, const string& searchString, vector<SearchResult>& results) {
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
            int totalFrequency = sqlite3_column_int(stmt, 1);

            if (pageName) {
                SearchResult result;
                result.pageName = pageName;
                result.totalFrequency = totalFrequency;
                results.push_back(result);  // Agregar el resultado al vector
            }
        }

        sqlite3_finalize(stmt);
        return true;
    }
    else {
        cerr << "Error al preparar la consulta de búsqueda: " << sqlite3_errmsg(db) << endl;
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
        if (sqlite3_open("database.db", &db) != SQLITE_OK)
        {
            cerr << "Error al abrir la base de datos" << endl;
            return false;
        }

        // Vector para almacenar los nombres de las páginas en el orden deseado
        vector<SearchResult> results;

        // Crear tabla FTS e insterarle los datos
        if (!createFTSTable(db))
            cerr << "Error al crear la tabla FTS: " << sqlite3_errmsg(db) << endl;

        if(!insertDataIntoFTS(db))
            cerr << "Error al preparar la consulta de inserción: " << sqlite3_errmsg(db) << endl;

        if(!searchUsingFTS(db, searchString, results))
            cerr << "Error al buscar con FTS: " << sqlite3_errmsg(db) << endl;

        // Preparar la consulta para realizar la búsqueda con FTS5
        sqlite3_stmt* stmt;
        string query = R"(
            SELECT url, SUM(frequency) AS total_frequency
            FROM keyword_index
            WHERE keyword = ?
            GROUP BY url
            ORDER BY total_frequency DESC
        )";

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            // Enlaza el término de búsqueda a la consulta
            sqlite3_bind_text(stmt, 1, searchString.c_str(), -1, SQLITE_STATIC);

            // Ejecutar la consulta y obtener los nombres de las páginas en orden de frecuencia
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char* pageName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (pageName)
                    results.push_back(pageName);
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            cerr << "Error al preparar la consulta" << endl;
            sqlite3_close(db);
            return false;
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


/*
bool HttpRequestHandler::handleRequest(string url,
                                               HttpArguments arguments,
                                               vector<char> &response)
{
    string searchPage = "/search";
    if (url.substr(0, searchPage.size()) == searchPage)
    {
        string searchString;
        if (arguments.find("q") != arguments.end())
            searchString = arguments["q"];

        // Header
        string responseString = string("<!DOCTYPE html>\
<html>\
\
<head>\
    <meta charset=\"utf-8\" />\
    <title>EDAoogle</title>\
    <link rel=\"preload\" href=\"https://fonts.googleapis.com\" />\
    <link rel=\"preload\" href=\"https://fonts.gstatic.com\" crossorigin />\
    <link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;800&display=swap\" rel=\"stylesheet\" />\
    <link rel=\"preload\" href=\"../css/style.css\" />\
    <link rel=\"stylesheet\" href=\"../css/style.css\" />\
</head>\
\
<body>\
    <article class=\"edaoogle\">\
        <div class=\"title\"><a href=\"/\">EDAoogle</a></div>\
        <div class=\"search\">\
            <form action=\"/search\" method=\"get\">\
                <input type=\"text\" name=\"q\" value=\"" +
                                       searchString + "\" autofocus>\
            </form>\
        </div>\
        ");

        // YOUR JOB: fill in results
        float searchTime = 0.1F;
        vector<string> results;

        // Print search results
        responseString += "<div class=\"results\">" + to_string(results.size()) +
                          " results (" + to_string(searchTime) + " seconds):</div>";
        for (auto &result : results)
            responseString += "<div class=\"result\"><a href=\"" +
                              result + "\">" + result + "</a></div>";

        // Trailer
        responseString += "    </article>\
</body>\
</html>";

        response.assign(responseString.begin(), responseString.end());

        return true;
    }
    else
        return serve(url, response);

    return false;
}
*/
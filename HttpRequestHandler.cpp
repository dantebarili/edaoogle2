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
#include <algorithm>

#include "HttpRequestHandler.h"
#include <sstream>

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

    const char* comandoCrearTablaFts = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS keyword_index_fts USING fts5(keyword, URL, frequency);
    )";

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, comandoCrearTablaFts, -1, &stmt, nullptr) == SQLITE_OK) {

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

    const char* comandoInsertarDatos = R"(
        INSERT INTO keyword_index_fts (keyword, URL, frequency)
        SELECT keyword, url, frequency FROM keyword_index;
    )";

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, comandoInsertarDatos, -1, &stmt, nullptr) == SQLITE_OK) {

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

        cerr << "Error al preparar la insercion de datos: " << sqlite3_errmsg(db) << endl;
        return false;

    }
}

bool searchUsingFTS(sqlite3* db, const string& searchString, vector<string>& results) {

    std::stringstream ss(searchString);
    std::string word;
    std::map<string, int> urlFrequencies;
    std::multimap<int, string> resultsMap;
    std::string comandoOrdenarUrlsContinuacion;

    urlFrequencies.clear();

    const char* comandoOrdenarUrls = R"(
        SELECT URL, frequency
        FROM keyword_index_fts
        WHERE keyword MATCH ?
    )";

    // Construir la consulta para las palabras clave separadas por OR
    while (ss >> word) {
        if (!comandoOrdenarUrlsContinuacion.empty()) {
            comandoOrdenarUrlsContinuacion += " OR ";
        }
        comandoOrdenarUrlsContinuacion += "\"" + word + "\"";
    }

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, comandoOrdenarUrls, -1, &stmt, nullptr) == SQLITE_OK) {

        sqlite3_bind_text(stmt, 1, comandoOrdenarUrlsContinuacion.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {

            const char* urlPagina = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));    // url de la pagina que coincide con la entrada
            int frequency = sqlite3_column_int(stmt, 1);    // frecuencia de la palabra actual en esa url

            if (urlPagina) {
                string url(urlPagina);
                urlFrequencies[url] += frequency;  // Acumula la frecuencia por URL
            }
        }

        sqlite3_finalize(stmt);
    }
    else {

        cerr << "Error al buscar con FTS: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    // Paso a otro mapa donde la clave sea la frecuencia
    for (const auto& item : urlFrequencies) {
        resultsMap.insert({item.second, item.first});
    }  

    for (auto it = resultsMap.rbegin(); it != resultsMap.rend(); ++it) {
        results.push_back(it->second);
        cout << "URL: " << it->second << ", Total Frequency: " << it->first << endl;
    }

    const char* comandoLimpiarTablaFTS = "DELETE FROM keyword_index_fts;";
    sqlite3_stmt* stmt3;
    if (sqlite3_prepare_v2(db, comandoLimpiarTablaFTS, -1, &stmt3, nullptr) == SQLITE_OK) {

        if (sqlite3_step(stmt3) != SQLITE_DONE) {

            cerr << "Error al limpiar el contenido de la tabla FTS: " << sqlite3_errmsg(db) << endl;

        }
        else {

            cout << "Contenido de la tabla FTS limpiado exitosamente." << endl;

        }

        sqlite3_finalize(stmt3);

    }
    else {

        cerr << "Error al preparar la consulta DELETE: " << sqlite3_errmsg(db) << endl;

    }

    return true;
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
        
        // para que se puedan clickear las paginas
        for (auto& result : results) {
            responseString += "<div class=\"result\"><a href=\"/wiki/" + result + /*".html\">" + result + */"</a></div>";
        }/*
        for (auto& result : results)
            responseString += "<div class=\"result\"><a href=\"#\">" + result + "</a></div>";
*/
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

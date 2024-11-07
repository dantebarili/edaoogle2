/**
 * @file mkindex.cpp
 * @author Marc S. Ressl
 * @brief Makes a database index
 * @version 0.3
 *
 * @copyright Copyright (c) 2022-2024 Marc S. Ressl
 */

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <filesystem>

#include <sqlite3.h>

using namespace std;

map<string, int> extraerPalabras(const std::string& archivo);
void guardarPalabrasEnDatabase(sqlite3* db, const map<string, int>& frecuenciaPalabras, const string& url);

static int onDatabaseEntry(void* userdata,
	int argc,
	char** argv,
	char** azColName)
{
	cout << "--- Entry" << endl;
	for (int i = 0; i < argc; i++)
	{
		if (argv[i])
			cout << azColName[i] << ": " << argv[i] << endl;
		else
			cout << azColName[i] << ": " << "NULL" << endl;
	}

	return 0;
}

int main(int argc, const char* argv[])
{
	/*------------CREACION Y CONFIGURACION DE LA BASE DE DATOS------------*/
	sqlite3* db;
	char* errMsg = 0;
	const char* sql;

	// Abrir la base de datos
	if (sqlite3_open("C:/Users/dante/OneDrive/Documentos/git/edaoogle2/search_index.db", &db)) {
		cout << "Error al abrir la base de datos: " << sqlite3_errmsg(db) << endl;
		return 1;
	}

	// Crear tabla para almacenar palabras clave, URLs y frecuencia de aparición
	sql = "CREATE TABLE IF NOT EXISTS keyword_index ("
		"id INTEGER PRIMARY KEY, "
		"keyword TEXT NOT NULL, "
		"url TEXT NOT NULL, "
		"frequency INTEGER NOT NULL);"; // Columna para frecuencia

	if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
		cout << "Error al crear la tabla: " << errMsg << endl;
		sqlite3_free(errMsg);
	}

	// Crear índice en la columna de palabras clave para agilizar las búsquedas
	sql = "CREATE INDEX IF NOT EXISTS idx_keyword ON keyword_index(keyword);";

	if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
		cout << "Error al crear el índice: " << errMsg << endl;
		sqlite3_free(errMsg);
	}

	/*------------FIN DE LA CREACION Y CONFIGURACION DE LA BASE DE DATOS------------*/

	/*------------MANIPULACION DE ARCHIVOS Y RELLENO DE LA BASE DE DATOS------------*/
	string path = "C:/Users/dante/OneDrive/Documentos/git/edaoogle2/www/wiki";

	// Comprobamos si la ruta existe
	if (!filesystem::exists(path)) {
		std::cerr << "La carpeta no existe." << std::endl;
		return 1;
	}

	// Iteramos sobre los archivos en la carpeta
	for (const auto& entrada : filesystem::directory_iterator(path)) {

		const string archivoPath = entrada.path().string();
		const string archivoNombre = entrada.path().filename().string();
		map<string, int> mapa = extraerPalabras(archivoPath);
		guardarPalabrasEnDatabase(db, mapa, archivoNombre);

	}
	/*------------FIN DE MANIPULACION DE ARCHIVOS Y RELLENO DE LA BASE DE DATOS------------*/

	cout << "Índice de búsqueda creado, y datos insertados exitosamente." << endl;

	// Cerrar la base de datos
	sqlite3_close(db);
	return 0;
}

map<string, int> extraerPalabras(const string& nombreArchivo) {
	ifstream archivo(nombreArchivo);
	map<string, int> frecuenciaPalabras;
	string linea;
	char brackets = 0;

	// Leer el archivo línea por línea
	while (getline(archivo, linea)) {
		string palabra;

		// Recorrer cada carácter en la línea
		for (char& ch : linea) {
			if (ch == '<') {
				brackets = 1;
				continue;
			}
			else if (ch == '>') {
				brackets = 0;
				continue;
			}

			// Si estamos dentro de una etiqueta, ignoramos el carácter
			if (brackets == 1) {
				continue;
			}

			if (isalpha(static_cast<unsigned char>(ch))) {
				palabra += tolower(static_cast<unsigned char>(ch));
			}
			else if (!palabra.empty()) {
				// Si no es una letra y tenemos caracteres en la palabra, pasamos a terminar de evaluarla
				frecuenciaPalabras[palabra]++;
				palabra.clear(); // Reiniciar la palabra
			}
		}

		// Captura la última palabra en la línea (por si el ultimo caracter fue una letra)
		if (!palabra.empty()) {
			frecuenciaPalabras[palabra]++;
		}
	}

	return frecuenciaPalabras;
}

void guardarPalabrasEnDatabase(sqlite3* db, const map<string, int> &frecuenciaPalabras, const string& url) {
	sqlite3_stmt* stmt;
	const char* sql = "INSERT INTO keyword_index (keyword, url, frequency) VALUES (?, ?, ?);";

	// Preparar la declaración
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
		cout << "Error al preparar la declaración: " << sqlite3_errmsg(db) << endl;
		return;
	}

	for (const auto& pair : frecuenciaPalabras) {
		string palabra = pair.first;
		int frecuencia = pair.second;

		// Bind de los valores a la declaración preparada
		sqlite3_bind_text(stmt, 1, palabra.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, frecuencia);

		// Ejecutar la declaración
		if (sqlite3_step(stmt) != SQLITE_DONE) {
			cout << "Error al insertar en la base de datos: " << sqlite3_errmsg(db) << endl;
		}

		// Resetear la declaración para el siguiente uso
		sqlite3_reset(stmt);
	}

	// Liberar la declaración preparada
	sqlite3_finalize(stmt);
}
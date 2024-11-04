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
	if (sqlite3_open("search_index.db", &db)) {
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
	string path = "C:/Users/Juani/Source/Repos/edaoogle2/www/wiki";

	// Comprobamos si la ruta existe
	if (!filesystem::exists(path)) {
		std::cerr << "La carpeta no existe." << std::endl;
		return 1;
	}

	// Iteramos sobre los archivos en la carpeta
	for (const auto& entrada : filesystem::directory_iterator(path)) {

		const string archivo_path = entrada.path().string();
		const string archivo_nombre = entrada.path().filename().string();
		map<string, int> mapa = extraerPalabras(archivo_path);
		guardarPalabrasEnDatabase(db, mapa, archivo_nombre);

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

	// Leer el archivo línea por línea
	while (getline(archivo, linea)) {
		string palabra;

		// Recorrer cada carácter en la línea
		for (char& ch : linea) {
			if (isalpha(ch)) {
				// Si es una letra, seguimos contruyendo la palabra
				palabra += tolower(ch);
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
	char* errMsg = 0;
	for (const auto& pair : frecuenciaPalabras) {
		string palabra = pair.first;
		int frecuencia = pair.second;

		// Preparar consulta SQL
		string sql = "INSERT INTO keyword_index (keyword, url, frequency) VALUES ('" +
			palabra + 
			"', '" + 
			url + 
			"', " + 
			frecuencia + ");";

		// Ejecutar consulta
		if (sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK) {
			cout << "Error al insertar en la base de datos: " << errMsg << endl;
			sqlite3_free(errMsg);
		}
	}
}
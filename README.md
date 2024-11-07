Este trabajo está dividido en dos etapas principales, la etapa de creación de la base de datos, y la etapa de la búsqueda de las páginas en la base de datos.

Creación de la base de datos:
Para crear la base de datos utilizamos la librería SQLite3, la cual nos permitió correr comandos de SQL en el mismo código. Se creó una tabla con 4 columnas, donde se ingresaba la palabra clave,
la página de donde salió y la cantidad de veces que aparecía en dicha página. Este proceso lo logramos automatizar mediante un algoritmo (plasmado en la función extraerPalabras) que solo agarra 
el texto de los archivos html, y otro algoritmo que recorre la carpeta donde se encuentran dichos archivos html. Por último, mediante la función guardarPalabrasEnDataBase, guardamos todas las
palabras de los textos en la tabla.

Busqueda de páginas en la base de datos:
El algoritmo de búsqueda se dividió en 3 funciones.
-crateFTSTable: En esta primera función se crea la tabla FTS, con la cuál podemos ordenar las urls de una manera mas rápida y eficiente.
-searchUsingFTS: Una vez creada la tabla, había que pasarle los datos de la tabla anterior a esta, esta función cumple ese propósito.
-searchUsingFTS:

Cómo configurar el programa para que funcione:
-mkindex.cpp:
  Linea 48: ingresar el path completo hasta la carpeta edaoogle2 e incluir el nombre de la base de datos (search_index.db).
  Linea 76: ingresar el path completo hasta la carpeta wiki.
-HtppRequestHandler.cpp:
  Linea 192: poner el path hasta la carpeta edaoogle2 e incluir el nombre de la base de datos (search_index.db).
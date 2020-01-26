<?php
include_once "config.php";

function dbConnect()
{
    if (DB_ENGINE == "sqlite")
    {
        // set up the SQLite database object
        $db = new PDO('sqlite:' . DB_FILE);
    }
    else if (DB_ENGINE == "mysql")
    {
        // set up the MySQL database object
        $db = new PDO('mysql:dbname=' . DB_SCHEMA . ';host=' . DB_HOST,
                      DB_USERNAME, DB_PASSWORD);
    }

    // return the connection
    return $db;
}

?>
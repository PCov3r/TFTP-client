# TFTP-client

Implémentation d'un client TFTP simple.
<br> <br>

## Client TFTP
Ce dernier a besoin au minimum de l'adresse IP du serveur vers lequel la requête est dirigée et du fichier à envoyer/recevoir.
Il est également possible de spécifier:
- Le port auquel on souhaite s'adresser en ajoutant `:port` après l'adresse ip
- La taille des blocks de donnée en spécifiant cette dernière `blksize` à la fin de la requête

On appelle ainsi le programme comme suit: `./xxxtftp addresse[:port] fichier [blksize]`

Si aucun port n'est spécifié, le port par défaut **69** est utilisé. <br>
Si aucune taille de bloc n'est spécifiée, la taille par défaut **512o** est utilisée.


Les programmes de base, sans l'option blksize se trouvent [ici](/Basic_implementation)
<br> <br>

## Test
Les tests ont été conduits sur un [serveur local](https://mohammadthalif.wordpress.com/2010/03/05/installing-and-testing-tftpd-in-ubuntudebian/) dont l'option `blksize` n'était pas implémentée. Ainsi, les tests avec cette option n'ont pu être effectués.

Des captures wireshark des différentes reuêtes, avec et sans l'option `blksize` peuvent être trouvées [ici]()


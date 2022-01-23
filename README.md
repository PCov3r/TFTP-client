# TFTP-client

Implémentation d'un client TFTP simple. <br> <br>
Ce dernier a besoin au minimum de l'adresse IP du serveur vers lequel la requête est dirigée et du fichier à envoyer/recevoir. <br>
Il est également possible de spécifier:
* Le port auquel on souhaite s'adresser en ajoutant `:port` après l'adresse ip
* La taille des blocks de donnée en spécifiant cette dernière `blksize` à la fin de la requête
<br> <br>
On appelle ainsi le programme comme suit:
`./xxxtftp addresse[:port] fichier [blksize]`
<br> 
Si aucun port n'est spécifié, le port par défaut 69 est utilisé.
<br>
Si aucune taille de bloc n'est spécifiée, la taille par défaut **512o** est utilisée.

Les programmes de base, sans l'option blksize se trouvent [ici](/Basic implementation/)

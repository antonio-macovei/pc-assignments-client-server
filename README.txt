Macovei Antonio Dan - 324CA
<antoniomacovei@yahoo.com>
05.05.2019

Protocoale de comunicatie - Tema 2 - Client - Server Application

CLIENT

Programul client primeste trei parametrii, CLIENT_ID, IP si PORT, cu ajutorul
carora se conecteaza la server. Acesta deschide un socket TCP catre IP-ul si
PORT-ul specificat, urmand ca mai apoi sa trimita un mesaj cu CLIENT_ID-ul
sau pentru a putea fi identificat in viitoarele conexiuni. In continuare,
cu ajutorul multiplexarii, se asculta mesaje atat pe socket-ul TCP, cat si
de la STDIN (tastatura).

In cazul in care programul primeste comenzi de la tastatura, exista 4 cazuri:
	1. subscribe <TOPIC> <SF> -> clientul trimite un mesaj catre server cu o
	structura 'command' in care este precizat numele comenzii, topic-ul la care
	se doreste abonarea si flag-ul SF (true sau false).
	2. unsubscribe <TOPIC> -> clientul trimite un mesaj asemanator cu cel de
	mai sus, folosind aceeasi structura 'command'.
	3. exit -> inchide socket-ul curent.
	4. orice alt input -> va afisa un mesaj informativ cu comenzile disponibile
	si sintaxa acestora.

In cazul in care programul primeste un mesaj de la server, acesta il va stoca
intr-o structura 'extended_message', care contine IP-ul si PORT-ul clientului
UDP de la care vine mesajul si mesajul efectiv (stocat la randul lui intr-o
structura 'message' cu campurile topic, type, payload). In continuare,
mesajul din 'message' este interpretat in functie de tipul de date si afisat
corespunzator, impreuna cu restul informatiilor primite.
Interpretarea datelor in functie de tip:
	- INT -> numarul a fost convertit din BIG-ENDIAN in LITTLE-ENDIAN prin
	concatenarea celor patru octeti in ordine inversa cu ajutorul shiftarii
	logice si a operatiei SAU logic. Apoi a fost adaugat semnul.
	- SHORT_REAL -> numarul a fost format din cei doi octeti folosind acelasi
	procedeu ca la INT, dupa care a fost formatat astfel incat virgula sa fie
	amplasata inainte de ultimele 2 cifre.
	- FLOAT -> numarul a fost format in acelasi mod ca cel de la INT, apoi
	a fost impartit la 10 la puterea aflata in primul octet de dupa numar.
	Apoi a fost adaugat semnul.
	- STRING -> datele sunt afisate neformatate.

SERVER

Programul server primeste un singur parametru, anume PORT-ul pe care va
deschide conexiunea. Acesta va deschide 2 socketi, unul folosind protocolul
UDP, prin intermediul caruia va primi mesaje de la un client UDP si le va
trimite mai departe, si unul folosind protocolul TCP, cu ajutorul caruia
va livra mesajele primite de la UDP unor clienti TCP. Socket-ul TCP va fi
setat in modul listen pentru a permite primirea de conexiuni din partea
clientilor si va avea activata optiunea TCP_NODELAY pentru a dezactiva
latenta de trimitere a mesajelor.
Deoarece programul ar trebui sa gestioneze mai multe tipuri de conexiuni,
este nevoie de multiplexarea intrarilor. Astfel, avem urmatoarele cazuri:
	- Primirea unei comenzi de la tastatura (STDIN). Singura comanda
	acceptata de server este cea de exit. Aceasta va inchide cei doi
	socketi (UDP si TCP), cat si conexiunea cu toti clientii conectati in
	acel moment. Pentru a asigura o inchidere sigura, serverul va trimite
	un mesaj cu comanda EXIT catre toti clientii TCP conectati, iar acestia
	vor inchide la randul lor conexiunea.
	Primirea oricarei alte comenzi va afisa un mesaj informativ cu comenzile
	disponibile.
	- Primirea unui mesaj din partea unui client UDP. In acest caz, serverul
	va primi mesajul in structura 'message', il va impacheta intr-o alta
	structura ('extended_message') impreuna cu IP-ul si PORT-ul clientului
	originar, apoi il va trimite mai departe catre clientii TCP care sunt
	abonati la topicul respectiv.
	- Primirea unei cereri de conexiune din partea unui client TCP. O cerere
	de conexiune venita pe socket-ul de listen este acceptata, adaugata in
	setul de file descriptori, dupa care se asteapta un mesaj de la client
	care contine ID-ul sau. Fiecare nou client este salvat intr-o strutura
	numita 'tcp_client', cu campurile CLIENT_ID, FD, subscriptions si
	stored_messages. In cazul in care un client s-a deconectat, acesta nu va
	fi scos din colectia de clienti, ci in eventualitatea unei reconectari,
	se va actualiza FD-ul sau si ii vor fi inaintate toate mesajele trimise de
	alti clienti cat timp acesta a fost offline, pentru topicurile pe care era
	setat flag-ul SF activ.
	Campul subscriptions contine o lista de perechi (topic, SF) cu toate
	topicurile la care este abonat clientul si cate un flag SF pentru fiecare
	astfel de topic. Din considerente de economie a memoriei, acest vector nu
	contine denumirea fiecarui topic, ci un ID unic, incrementat la adaugarea
	fiecarui topic nou, astfel incat sa nu existe denumiri duplicate.
	Campul stored_messages contine o lista de ID-uri ale mesajelor care trebuie
	transmise la reconectarea unui client care a fost offline si avea setat
	flag-ul SF pentru anumite topicuri. Din acelasi motiv ca mai sus, in acest
	vector vor fi pastrate doar ID-urile mesajelor in loc de acestea in
	intregime.
	Pentru a putea mapa legatura dintre ID si topicuri, respectiv ID si mesaje,
	am folosit alte doua structuri, 'topic_struct' si 'stored_message'.
	Tot pentru economisirea memoriei, 'stored_message' contine inca un camp
	numit pending_clients, care contorizeaza cati clienti mai trebuie sa
	primeasca acel mesaj. In momentul in care mesajul a fost trimis catre toti
	clientii ramasi, acesta va fi sters.
	- Primirea unei comenzi din partea unui client TCP. Clientii TCP pot
	trimite comenzi de subscribe, respectiv unsubscribe. Acestea vor fi
	interpretate de server, adaugand, respectiv eliminand topicuri din listele
	de subscriptii ale clientilor (campul subscriptions din tcp_client).
	
Mentiuni:
	- Programul functioneaza in parametrii normali, insa uneori exista niste
	erori care determina afisarea unui text gresit la reconectarea unui client
	TCP care trebuie sa primeasca mesaje stocate cat a fost offline. Problema
	cu aceste erori este ca nu sunt predictibile, aparand intamplator, chiar si
	la urmarea acelorasi pasi de mai multe ori. Un exemplu de mesaj eronat:
	:12849 - 7.0.0.1 - INT - 0
	Ca tehnica de protejare impotriva acestei probleme am incercat golirea
	(zeroizarea) zonei de memorie inainte de a receptioa un mesaj sau de a-l
	trimite, insa nu a rezolvat erorile.
	- Programul contine atat sanitizarea inputului de la tastatura, cat si
	verificarea valorii de retur dupa fiecare apel de sistem, tratand astfel
	erorile ce pot aparea.







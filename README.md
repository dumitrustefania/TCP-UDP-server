# Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

322CA - Bianca Ștefania Dumitru
Protocoale de comunicatii

Mai 2023
----------------------------------------------------------------------------------------------------
## Introducere

* Aplicatie client-server TCP si UDP pentru gestionarea mesajelor
  * programul implementeaza un server ce primeste mesaje de la clientii UDP
      si le transmite clientilor TCP conectati si abonati la mesaje
  * se folosesc socketi si multiplexarea I/O prin intermediul functiei poll(),
      atat in cadrul serverului, cat si al clientilor TCP, ce actioneaza ca subscriberi

## Ce contine proiectul?

* server.cpp - serverul unic, ce deschide 2 socketi de listen pentru clienti TCP si UDP
    si asteapta conexiuni. El dirijeaza intreaga functionalitate a aplicatiei, directionand
    datele primite de la clientii UDP catre clientii TCP abonati. Poate primi si comenzi de
    la stdin sau mesaje de tipul subscribe/unsubscribe de la clientii TCP conectati, de care
    tine cont pentru trimiterea personalizata a mesajelor.

* subscriber.cpp - implementarea unui client TCP, ce se conecteaza la server si
    comunica cu acesta prin trimiterea si primirea de mesaje. El poate primi, de asemenea,
    si comenzi de la stdin.

* common.cpp - contine functiile recv_all si send_all, wrappere peste recv si send din
    biblioteca de socketi TCP.

* helpers.h - contine macro-uri si 2 structuri utile: udp_packet si tcp_client.

## Cum am implementat?

Am inceput prin a pleca de la codul scris in laboratorul 7, despre protocolul TCP.
La fel ca la tema, aveam un server la care trebuiau conectati mai multi clienti TCP.
A fost utila mai ales partea de multiplexare cu poll() si folosirea API-ului
pentru socketii TCP. In common.cpp, am pastrat functiile implementate de mine,
recv_all si send_all, ce se asigura ca sunt primiti/trimisi toti octetii de catre functiile
send si recv. Ele sunt necesare pentru ca recv si send pot receptiona / trimite mai puțini
octeti decat am dat ca parametru.

Pornind de la scheletul laboratorului si rezolvarea mea, am preluat fisierele helpers.h,
common.cpp, common.h si am creat un 'schelet' in fisierele server.cpp si subscriber.cpp.

Pentru integrarea clientilor UDP, m-am folosit de o parte din scheletul laboratorului 5,
mai exact din fisierul cu serverul (folosirea API-ului de socketi UDP).

In server, pentru receptionarea datagramelor de la clientii UDP, am hotarat sa receptionez
mesajul sub forma de string pe care sa-l parsez apoi sub forma de structura de tip udp_packet
(definita in helpers.h). Am creat o functie care sa se ocupe de acest lucru, urmarind indicatiile
din enuntul temei, in functie de tipul de date primite.

Pentru trimiterea acestor mesaje catre clientii TCP, m-am gandit la mai multe modalitati. Prima
dintre ele era sa trimit un pachet care continea IP-ul si portul clientului UDP care trimisese mesajul,
pe langa pachetul (sub forma de char *, deci neparsat) primit de la clientul UDP. Acest lucru presupunea
mutarea functionalitatii de parsare mentionate anterior in interiorul clientului TCP si repetarea acestui
procedeu de catre fiecare client. Nu mi s-a parut ca acest lucru ar avea sens si am dorit separarea logicii
de formatare si trimitere a pachetului (de catre clientul UDP) de functionalitatea clientului TCP.
Astfel, am considerat ca ar avea mai mult sens trimiterea mesajului deja parsat sub forma de string de la
server catre clientul TCP, pentru ca acesta sa trebuiasca doar sa il afiseze la primire. 

Am lucrat apoi la functionalitatea de subscribe/unsubscribe. Am realizat ca este nevoie sa stochez
undeva separat toti clientii TCP, asa ca am creat structura tcp_client (in helpers.h), care stocheaza
toate datele importante despre un client TCP (id, fd, abonari, conectat). Asadar, la primirea unei cereri
de subscribe/unsubscribe, updatez vectorul de abonari al clientului TCP. De asemenea, am avut grija ca inaintea
trimiterii unui mesaj de la un client UDP sa verific pentru fiecare client ca este abonat.

Pentru functionalitatea de store-forward, am ales initial sa creez cate un fisier cu numele = id-ul clientului TCP,
pentru fiecare client TCP care se deconecta, si sa scriu acolo mesajele pentru trimitere ulterioara. Am realizat apoi
ca era un procedeu destul de complicat, fiind nevoie sa creez/deschid/scriu in/sterg fisiere constant, asa ca
optat pentru ceva mai simplu, crearea unui hashtable cu cheia = id-ul clientului deconectat si valoarea = coada
de mesaje ce vor trebui trimise la reconectare. Am considerat ca aceasta varianta ar fi mai rapida.

La primirea comenzii exit, m-am asigurat ca inchid toti clientii TCP conectati si apoi serverul.

## Resurse
* Enuntul temei - https://gitlab.cs.pub.ro/pcom/homework2-public/-/blob/main/Enunt_Tema_2_Protocoale_2022_2023.pdf
* Laboratoarele 5 si 7 - https://pcom.pages.upb.ro/labs/

## Feedback
Considerand ce am implementat, mi s-a parut o tema destul de simpla si draguta, la care m-am putut folosi
foarte mult de cunostintele si codul scrise in laboratoarele trecute. Am inteles mult mai bine cum functioneaza
poll si multiplexarea, dar si diferenta mai clara de implementare intre TCP si UDP, mai ales pe partea de
API de socketi.

Ceea ce nu mi-a placut este ca nu a fost explicat clar ce se doreste de la noi prin 'implementarea unui protocol la nivel
aplicatie' sau 'propriul protocol pentru comunicarea TCP'. Inca nu sunt sigura ca ceea ce am realizat se incadreaza
la aceste aspecte. Eu am decis sa trimit stringuri de la server la clientii TCP, desi puteam trimite si structuri mai
complicate, dar nu mi s-a parut ca ar aduce un mare plus de eficienta. Sper ca a fost bun rationamentul meu. Local,
toate testele imi trec.


/**
 * Program służy do wysyłania danych za pomocą pojedyńczego lasera
 * Transformuje tablicę bajtów do postaci bitowej i na jej postawie generuje sekwencję sygnałów wysyłanych przez laser
 * Przed każdym bajtem dodaje bit "1", co służy zminimalizowaniu wpływu błędów czasowych urządzenia Arduino
 * 
 * 
 * Program nie posiada znaków specjalnych pomiędzy 
 * 
 * 
 * Klawiatura 4 przyciskowa zmienia funkcjonalność programu
 * Przycisk 1 anuluje transmisję
 * Przyciski 2 i 3 wybierają dane do przesłania
 * Przycisk 4 anuluje transmisję i wysyła aktualne dane
 * 
 */



 /**
  * Plan działania:
  * Loop stale sprawdza inputy klawiatury
  * Gdy wykryje zmianę stanu (klawisze 2 i 3) nadpisuje tablicę z danymi, poprzednią/następną tablicą
  * Gdy wykryje przycisk 4, rozpoczyna procedurę wysyłania tablicy z danymi
  * Gdy podczas wysyłania (ale nie synchronizacji) naciśnięty zostanie przycisk 1, wysłany zostaje EOF transmisja jest zakończona
  * Program wraca do loopa i sprawdzania inputów
  */

int laserPin = 12;    //Pin laser
int klawPin_pocz=4; //Początek pinów klawiatury
int klawPin_kon=7; //Koniec pinów klawiatury

const int n = 3; //Ilość różnych wiadomości 
unsigned char wszystkie_dane[n][200] =   //Tablica stringów, max 200 znaków długści ze względu na ograniczoną pamięć.
 {"Litwo, Ojczyzno moja! ty jestes jak zdrowie; Ile cie trzeba cenic, ten tylko sie dowie, Kto cie stracil. Dzis pieknosc twa w calej ozdobie Widze i opisuje, bo tesknie po tobie.",
 "Test1234567890987654321tes",
 "Boooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooolean"};

unsigned char dane[200];//Tablica danych aktualnych
boolean bylHigh = false; //Sprawdzanie czy przycisk klawiatury został naciśnięty (niekoniecznie zwolniony)
boolean trwa_wysylanie = false; //Czy w obecnej chwili trwa wysyłanie
int stan = 0; //Stan aktualny programu, 0 - stop; 1 - napis nr 1; n - napis nr n

int ile_powtorz = 1;   //Ile razy sekwencja danych ma zostać wysłana
int ile_na_sek = 60;    //Ile bitów (wraz z dodaną jedynką) zostanie wysłanych w czasie jednej sekundy, max 90
int ile_cykli_synchronizacji = 100;   //Ile cyklów (aktywacji i dezakctywacji lasera) ma zawierać faza synchronizacji -- Musi być kompatybilne z programem odbierającym
int timeout = 5000;   //Jaki czas ma upłynąć po ukończeniu synchronizacji -- Musi być kompatybilne z programem odbierającym
int przerwa_transmisji = 5000; //Przerwa pomiędzy transmisjami -- Musi być kompatybilne z programem odbierającym



/**
 * Funkcja inicjuje piny
 */

void setup() {
  pinMode(laserPin, OUTPUT);  
  //int ile_danych = sizeof(dane)/sizeof(dane[0]);
  for(int i=klawPin_pocz; i<=klawPin_kon; i++)
  {
  pinMode(i, INPUT_PULLUP);
  }
  bylHigh = false;
   Serial.begin(9600);
}



/**
 * Sprawdza inputy
 */
void loop() {
    tryby();
}



/**
   * Wywołuje funkcje i zmienia stan programu, czyli dane do przeslania
   */

  int tryby() {
    int piny[] = {0,0,0,0};
    /*
     * Sprawdza czy piny zostały naciśnięte
     */ 
    for(int i=klawPin_pocz; i<=(klawPin_kon); i++) {
      piny[i-klawPin_pocz]=zarejestrujNacisk(i);
    }

    //stan=1;
    if (piny[0]==1) {
      //Anuluj, STAPH
       if (trwa_wysylanie) {
        //EOT, przerwij wysylanie
        trwa_wysylanie=false;
        delay(przerwa_transmisji);
        return 1;
        }
      }
    else if (piny[1]==1) {
      //Poprzednia wiadomość
      if (stan==0) {
        stan = n;
        }
      else {
        stan--;
        }
      }
    else if (piny[2]==1) {
      //Następna wiadomość
      if (stan==n) {
        stan = 0;
        }
       else {
        stan++;
        } 
      }
    else if (piny[3]==1) {
      //Anuluj + Wyślij
      if (trwa_wysylanie) {
        //EOT, przerwij wysylanie
        delay(przerwa_transmisji);
        return 1;
        }
        if (stan!=0) {
        trwa_wysylanie=true;
        zamien_tablice();
        procedura();
        }
      }
    return 0;
    }




 /**
   * Sprawdza czy przycisk został naciśnięty i zwolniony
   */
int reading = 0;
boolean cz= false;
int zarejestrujNacisk(int pin) {
  cz=false;
  if (digitalRead(pin)==HIGH && reading==1) {
      cz = true;
      }
  if (digitalRead(pin)==LOW) {
    reading = 1;
    }
  else {
    reading = 0;
    }
    
    if (cz) {return 1;}
    else {return 0;}


  }



  
/**
 * Przepisuje wariant danych do głównej tablicy
 */
void zamien_tablice() {
  for (int i = 0; i<1000; i++) {
    if (wszystkie_dane[stan-1][i]=='\0') {
      break;
      }
      dane[i] = wszystkie_dane[stan-1][i];
    }
  }


  

/**
 * Wysyła aktualną tablicę danych
 * Uruchamia całą procedurę wysyłania
 */
void procedura() {
  int ile_danych = sizeof(dane)/sizeof(dane[0]);
  synchronizacja_ini();
  synchronizacja(ile_cykli_synchronizacji, ile_na_sek, timeout);
  przesylanie_ini();
  przesylanie(ile_powtorz, ile_na_sek, dane, ile_danych );
  delay(przerwa_transmisji);
  }



/**
 * Pojedyńcza aktywacja lasera oznajmia odbiornikowi początek synchronizacji 
 */
void synchronizacja_ini () {
  delay(1000);
  digitalWrite(laserPin, HIGH);
  delay(250);
  digitalWrite(laserPin, LOW);
  }


  
/**
 * Pojedyńcza aktywacja lasera oznajmia odbiornikowi początek przesyłania danych 
 */
void przesylanie_ini() {
  digitalWrite(laserPin, HIGH);
  delay(100);
  }



/**
 * Funckja aktywuje i dezaktywuje laser określoną liczbę razy
 * Pomaga w translacji rozumienia czasu na obu urządzeniach 
 * (Wszczególnie przydatna gdy urządzenia pracują z różną częstotliwością zegara)
 */

void synchronizacja(int ilosc_prob, int ile_na_sek, int timeout) {
   int sleep = (1000/ile_na_sek);  
   boolean zmiana = true;
   for (int i = 0; i< ilosc_prob*2; i++) {
     if (zmiana) {
      digitalWrite(laserPin, LOW);
      zmiana = false;
      }
      else {
        digitalWrite(laserPin, HIGH);
        zmiana = true;
        }
     delay(sleep);
    }
    digitalWrite(laserPin, LOW);
    delay(timeout);
  }



/**
 * Funkcja wywołuje wyslij_byte określoną dla każdego bajta z tablicy określoną ilość razy
 * Po zakończeniu transmisji wysyła znak EOT (bajt o wartości 4)
 * Następnia aktywuje i dezaktywuje laser w celu zmiany stanu koniecznej do interpretacji ostatniego bajta przez odbiornik
 */

void przesylanie (int ilosc_prob, int ile_na_sek, unsigned char dane[], int ile_danych) {
     trwa_wysylanie=true;
     unsigned char zakoncz = 4;   //znak EOT
     int sleep = (1000/ile_na_sek);  

     /*
      * Sprawdza czy transmisja powinna być anulowana, 
      * Jeżeli tak, kończy czytanie i transmitowanie charów
      * Wysyła EOT jako znak końca transmisji
      */
     boolean trwa = false;
     for (int i = 0; (i<ilosc_prob) && !trwa; i++){                                              
        for (int j=0; (j<ile_danych) && !trwa; j++) {
          if (dane[j]=='\0') {
            break;
            }
            wyslij_byte(sleep, dane[j]);    //Wyślij bajt z pozycji tablicy o indeksie j   
            if (tryby()) {
              trwa = true;
              digitalWrite(laserPin, LOW);
              goto end;
              } 
        }
     }
     end:
     wyslij_byte(sleep, zakoncz);   //Wyślij EOT

     /*
      * Konieczna zmiana stanu lasera
      */
     digitalWrite(laserPin, HIGH);
     delay(100);
     digitalWrite(laserPin, LOW);
     trwa_wysylanie=false;
  }



/**
 * Funkcja wysyła sygnał za pomocą lasera na podstawie odebranego bajta
 * Przed każdym bajtem wysyła bit o wartośći 1
 */

void wyslij_byte (int sleep, unsigned char bajt) {
    unsigned char maska = 128;
    unsigned char temp;   //Wartość bita na danej pozycji w bajcie
    digitalWrite(laserPin, HIGH);   //Bit o wartości 1
    delay(sleep);
    for (int i = 0; i<8; i++) {
      temp = bajt;
      temp = temp & maska;
      if (temp>0) {
        digitalWrite(laserPin, HIGH);
        }
      else {
        digitalWrite(laserPin, LOW);
        }
      maska=maska>>1;
      delay(sleep);
    }  
  }

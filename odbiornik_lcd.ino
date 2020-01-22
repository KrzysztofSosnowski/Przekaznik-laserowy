/**
 * Program odbiera i interpretuje dane bitowe przesyłane przez pojedyńczy laser w postaci stanu aktywancji lasera w czasie 
 * O stanie aktywacji lasera świadczą zmiany natężenia światła odbieranego przez podłączony czujnik - fotokomórkę 
 * Odebrane dane są interpretowane jako podstawowy kod ASCII (7 bitowy)
 * 
 * Program nasłuchuje pierwszego sygnału aktywnego po czym przechodzi do fazy synchronizacji (synchronizuj)
 * Na podstawie synchronizacji, program określa średni okres pojedyńczego sygnału (srednia)
 * Następnie program nasłuchuje kolejnego sygnału aktywnego (sluchaj_ini) i przechodzi do odbierania wiadomości (sluchaj)
 * Na podstawie wyliczonego okresu pojedyńczego sygnału (srednia), program interpretuje odczyty czujnika jako kod bitowy,
 * który zostaje przetworzony na znaki ASCII (konwertuj)
 * Aby zredukować ilość błędów powstałą w wyniku losowych zmian okresu zmian stanu lasera, przed każdym bajtem zostaje dodana jedynka,
 * którą program pomija
 * Powstałe znaki ASCII są wyświetlane na monitorze portu szeregowego.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

int sensorPin = 2;  //Pin dancyh sensora
int ile_cykli_synchronizacji=100;   //Oczekiwana ilość sygnałów aktywnych odebranych podczas synchronizacji -- Musi być kompatybilne z programem odbierającym
int timeout = 4000;   //Po jakim czasie bez odbierania sygnału synchronizacja zostanie zakończona -- Musi być kompatybilne z programem odbierającym (zmienjszone o sekundę)
int opoznienie_pomiedzy_transmisjami = 5000;    //Po jakim czasie od zakończenia transmisji, program będzie nasłuchiwał kolejnej

int value = 0;
unsigned long zaczeto;

void setup() {
  Serial.begin(9600);
  //lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  //lcd.autoscroll();
}


/**
 * Główna pętla, nasłuchuje początku poszczególnych transmisji i aktywuje kolejne fazy działania programu
 */

void loop() {
  value = analogRead(sensorPin);
  if (value<50) {
    zaczeto=millis();
    delay(520);
    int srednia = synchonizuj();                //Wywołuje synchronizację, która znajduje średni okres trwania jednego sygnału
    sluchaj_ini(srednia);                       //Wywołuje fazę synchronizacji
    delay(opoznienie_pomiedzy_transmisjami);    //Po zakończeniu transmisji, czeka dany okres czasu i wysłuchuje następnej
   }
}

/**
 * Nasłuchuje początku fazy sluchaj i ją inicjalizuje 
 */

int sluchaj_ini (int srednia) {
  lcd.clear();
  while (true) {
    value = analogRead(sensorPin);
    if (value<50) {
      delay(100);
      sluchaj(srednia);
      return 0;
    }    
  }
 }

/**
 * Przeprowadza fazę synchronizacji i znajduje uśredniony okres trwania pojedyńczego sygnału
 */

int synchonizuj() {
  Serial.println("---Synchronizacja rozpoczęta---");
  Serial.println();
  
  int roznica=0;    //Okres pomiędzy zmianami stanu aktywacji lasera
  int czas_jedynki=0;   //Suma okresów aktywnych lasera
  unsigned long pocz_czas=millis();   //Timestamp 1
  unsigned long kon_czas=millis();   //Timestamp 2
  int poprzedni=0;    //Wartość poprzedniego odczytu
  int srednia=0;    //Uśredniony okres trwania pojedyńczego sygnału
  
  Serial.print("Czas rozpoczęcia: ");
  Serial.println(millis());
  Serial.print("Timeout: ");
  Serial.println(timeout);
  Serial.print("Ilość przewidzianych sygnałów: ");
  Serial.println(ile_cykli_synchronizacji);

  /**
   * Jeżeli w ciągu czasu mniejszego niż timeout nastąpiła zmiana stanu, kontytnuuje nasłuchiwanie
   * Odnajduje czas zmian stanu i na ich podstawie kalkuluje okres sygnału oraz liczy sumę okresów stanu aktywnego
   */
  
  while(millis()-kon_czas < timeout) {   
    value = analogRead(sensorPin);
    if (value<50) {
      value = 1;
    }
    else {
      value = 0;
    }
    if (value!=poprzedni && value==1) {
      pocz_czas = millis();
    }
    if (value!=poprzedni && value==0) {
      kon_czas = millis();
      roznica = kon_czas-pocz_czas;
      czas_jedynki += roznica;
    }
   poprzedni = value;
  }

  /**
   * Znajduje uśredniony okres trwania pojedyńczego sygnału i go zwraca
   */
  srednia = czas_jedynki / ile_cykli_synchronizacji;
  if (czas_jedynki<1500) {
    Serial.println("---Uwaga---");
    Serial.println("Wychwycono zbyt mało sygnałów do przeprowadzenia poprawnej synchronizacji");
  }
  Serial.print("Suma okresów trwania sygnałów: ");
  Serial.println(czas_jedynki);
  Serial.print("Średni czas trwania jednego sygnału: ");
  Serial.println(srednia);
  if (srednia < 15) {
    Serial.println("---Uwaga---");
    Serial.println("Połączenie nie jest stabilne dla częstotliwości wysyłania danych większej od 50 bitów na sekundę"); 
  }
  Serial.println();
  Serial.println("---Synchronizacja zakończona---");
  srednia--;//korekcja średniej
  return srednia;
}


/** 
 *  Odbiera transmisję i wywołuje funkcję konwertuj
 *  Jeżeli odebrany bajt to EOT, kończy odbieranie transmisji
 */

void sluchaj(int srednia) {
  Serial.println("");
  Serial.println("---Rozpoczęcie nasłuchiwania---");
  Serial.println("");
  
  int roznica=0;    //Okres pomiędzy zmianami stanu aktywacji lasera
  unsigned long pocz_czas=millis();   //Timestamp 1
  unsigned long kon_czas=millis();   //Timestamp 2
  int poprzedni=0;    //Wartość poprzedniego odczytu
  int tab[9];   //Tablica odebranych bitów
  int temp;
  char bajt=0;    //Bajt zwrócony przez funkcję konwertuj
  int reszta;   //Niepełny okres jednego bitu
  int n = 0;    //Licznik wypełnienia tablicy
  int niepewnosc = srednia/2;   //Jeżeli niepełny okres bitu przekracza połowę okresu bitu, uznajemy że bit został wysłany

  /*
   * Odbiera transmisję w postaci odczytów czujnika i zamienia ją na tablicę bitów, którą przekazuje funkcji konwertuj
   * Aktywna aż do wystąpienia znaku EOT
   */
  while(((millis()-kon_czas)<=5000)&&((millis()-pocz_czas)<=5000)&&(bajt!=4)) { 
    value = analogRead(sensorPin);
    if (value<50) {
      value = 1;
    }
    else {
      value = 0;
    }
    if (value!=poprzedni && value==1) {
      pocz_czas = millis();
      roznica = pocz_czas-kon_czas;
      temp = roznica/srednia;
      reszta = roznica-(temp*srednia);
      for (int i = 0; i<temp; i++) {
        tab[n]=poprzedni;
        n++;
        if (n==9) {
          bajt = konwertuj(tab);
          n=0;
        }
      }
      if(reszta>(niepewnosc)) {
        tab[n] = poprzedni;
        n++;
        if (n==9) {
          bajt = konwertuj(tab);
          n=0;
        }
      }
      else {
      }
    }
    if (value!=poprzedni && value==0) {
      kon_czas = millis();
      roznica = kon_czas-pocz_czas;
      temp = roznica/srednia;
      reszta = roznica-(temp*srednia);
      for (int i = 0; i<temp; i++) {
        tab[n]=poprzedni;
        n++;
        if (n==9) {
          bajt = konwertuj(tab);
          n=0;
        }
      }
      if(reszta>(niepewnosc)) {
        tab[n] = poprzedni;
        n++;
        if (n==9) {
          bajt = konwertuj(tab);
          n=0;
        }
      }
      else {
      }
    }
   poprzedni = value;
  }
  
  Serial.println();
  Serial.println();
  Serial.println("---Transmisja zakończona---");
  Serial.println("");
  Serial.print("Czas trwania transmisji[ms]: ");
  Serial.println(millis()-zaczeto);
  
}

/**
 * Konwertuje dostarczoną tablicę bajtów na bajty, pomijając pierwszą jedynkę
 * Zwraca utworzony bajt i wyświetla bajt w postaci znaku ASCII na monitorze portu szeregowego
 */
int pozycja=0;
byte konwertuj(int tablica[9]) {
  unsigned char bajt = 0;
  unsigned char maska = 128;
  unsigned char tempmaska=0;
  for (int i = 1; i <= 8; i++) {
    tempmaska=maska*tablica[i];
    bajt = bajt + tempmaska;
    maska=maska>>1;
   }
  if (bajt!=4) {    //Jeżeli odebrany bajt to EOT, nie wyświetla go
    Serial.write(bajt);
    pozycja++;
    lcd.write(bajt);
    if (pozycja==32) {
      lcd.setCursor(0,0);
      pozycja=0;
      }
    else if (pozycja==16) {
      lcd.setCursor(0,1);
      }
   }
  return bajt;
}

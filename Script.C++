#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int PIN_TRIG = 5;
const int PIN_ECHO = 18;
const int PIN_BUZZER = 19;
const int PIN_LED_VERDE = 25;
const int PIN_LED_AMARELO = 26;
const int PIN_LED_VERMELHO = 27;
const int PIN_BOTAO_SOS = 32;
const int PIN_BOTAO_MODO = 33;

const int LIMITE_ATENCAO = 120; 
const int LIMITE_PERIGO = 60; 

Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool telaDisponivel = false;
bool audioAtivo = true;
bool sosAtivo = false;
int distanciaCm = -1;

unsigned long ultimaMedicao = 0;
unsigned long ultimoDesenho = 0;
unsigned long ultimoSom = 0;
unsigned long ultimoLog = 0;

enum NivelAlerta { SEM_LEITURA, LIVRE, ATENCAO, PERIGO };
NivelAlerta nivelAtual = SEM_LEITURA;
NivelAlerta nivelAnterior = SEM_LEITURA;

struct Botao 
{
  int pino;
  bool leituraAnterior = HIGH;
  bool estadoEstavel = HIGH;
  unsigned long alteradoEm = 0;

  bool foiPressionado() {
    bool leitura = digitalRead(pino);
    if (leitura != leituraAnterior) alteradoEm = millis();
    leituraAnterior = leitura;

    if (millis() - alteradoEm >= 40 && leitura != estadoEstavel) {
      estadoEstavel = leitura;
      if (estadoEstavel == LOW) return true;
    }
    return false;
  }
};

Botao botaoSos{PIN_BOTAO_SOS};
Botao botaoModo{PIN_BOTAO_MODO};

int medirDistancia() 
{
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long tempoEco = pulseIn(PIN_ECHO, HIGH, 26000);
  if (tempoEco == 0) return -1;
  return (int)(tempoEco / 58);
}

NivelAlerta classificarDistancia(int cm) 
{
  if (cm < 0) return SEM_LEITURA;
  if (cm <= LIMITE_PERIGO) return PERIGO;
  if (cm <= LIMITE_ATENCAO) return ATENCAO;
  return LIVRE;
}

const char* nomeNivel(NivelAlerta nivel) 
{
  switch (nivel) {
    case LIVRE: return "LIVRE";
    case ATENCAO: return "ATENCAO";
    case PERIGO: return "PERIGO";
    default: return "SEM LEITURA";
  }
}

void atualizarLeds() 
{
  bool piscaRapido = ((millis() / 180) % 2) == 0;
  bool piscaLento = ((millis() / 450) % 2) == 0;

  bool verde = !sosAtivo && nivelAtual == LIVRE;
  bool amarelo = !sosAtivo && (nivelAtual == ATENCAO || nivelAtual == SEM_LEITURA) && piscaLento;
  bool vermelho = sosAtivo ? piscaRapido : (nivelAtual == PERIGO && piscaRapido);

  digitalWrite(PIN_LED_VERDE, verde ? HIGH : LOW);
  digitalWrite(PIN_LED_AMARELO, amarelo ? HIGH : LOW);
  digitalWrite(PIN_LED_VERMELHO, vermelho ? HIGH : LOW);
}

void atualizarSom() 
{
  if (!audioAtivo) return;

  unsigned long intervalo = 0;
  int frequencia = 0;
  int duracao = 0;

  if (sosAtivo) {
    intervalo = 350;
    frequencia = 2000;
    duracao = 140;
  } else if (nivelAtual == PERIGO) {
    intervalo = 270;
    frequencia = 1500;
    duracao = 105;
  } else if (nivelAtual == ATENCAO) {
    intervalo = 850;
    frequencia = 900;
    duracao = 85;
  }

  if (intervalo > 0 && millis() - ultimoSom >= intervalo) {
    tone(PIN_BUZZER, frequencia, duracao);
    ultimoSom = millis();
  }
}

void desenharTela() 
{
  if (!telaDisponivel) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(14, 1);
  display.println("CIVIA  /  GUARDIAN");
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  if (sosAtivo) {
    display.setTextSize(2);
    display.setCursor(5, 18);
    display.println("SOS ATIVO");
    display.setTextSize(1);
    display.setCursor(5, 43);
    display.println("Pedido simulado");
    display.setCursor(5, 54);
    display.println("Clique SOS p/ sair");
  } else {
    display.setTextSize(2);
    display.setCursor(5, 17);
    display.println(nivelAtual == SEM_LEITURA ? "SEM SINAL" : nomeNivel(nivelAtual));
    display.setTextSize(1);
    display.setCursor(5, 39);
    display.print("Dist.: ");
    if (distanciaCm < 0) display.println("sem leitura");
    else {
      display.print(distanciaCm);
      display.println(" cm");
    }
    display.setCursor(5, 53);
    display.print("Modo: ");
    display.print(audioAtivo ? "som + luz" : "visual");
  }

  display.display();
}

void registrarEstado() 
{
  if (nivelAtual != nivelAnterior || millis() - ultimoLog >= 4000) {
    Serial.print("[CIVIA] Estado: ");
    Serial.print(nomeNivel(nivelAtual));
    Serial.print(" | Distancia: ");
    Serial.print(distanciaCm);
    Serial.print(" cm | Audio: ");
    Serial.print(audioAtivo ? "ligado" : "desligado");
    Serial.print(" | SOS: ");
    Serial.println(sosAtivo ? "ativo" : "inativo");
    nivelAnterior = nivelAtual;
    ultimoLog = millis();
  }
}

void setup() 
{
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AMARELO, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_BOTAO_SOS, INPUT_PULLUP);
  pinMode(PIN_BOTAO_MODO, INPUT_PULLUP);

  Wire.begin(21, 22);
  telaDisponivel = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!telaDisponivel) {
    Serial.println("[CIVIA] OLED indisponivel. Alertas continuam ativos.");
  }

  Serial.println("=======================================");
  Serial.println("  CIVIA Guardian | Prototipo CitizenTech");
  Serial.println("  Ajuste a distancia no sensor HC-SR04.");
  Serial.println("  BOTAO AZUL: ativar/desativar audio.");
  Serial.println("  BOTAO VERMELHO: SOS simulado.");
  Serial.println("  SOS nao envia mensagens reais.");
  Serial.println("=======================================");
  desenharTela();
}

void loop() 
{
  if (botaoSos.foiPressionado()) {
    sosAtivo = !sosAtivo;
    noTone(PIN_BUZZER);
    ultimoSom = millis() - 1000;
    if (sosAtivo) {
      Serial.println("[CIVIA SOS] PEDIDO DE AJUDA GERADO (SIMULACAO).");
      Serial.println("[CIVIA SOS] Integracao com aplicativo e contato ainda nao implementada.");
    } else {
      Serial.println("[CIVIA SOS] Pedido de ajuda cancelado pelo usuario.");
    }
    desenharTela();
  }

  if (botaoModo.foiPressionado()) {
    audioAtivo = !audioAtivo;
    if (!audioAtivo) noTone(PIN_BUZZER);
    ultimoSom = millis() - 1000;
    Serial.println(audioAtivo ? "[CIVIA] Modo multimodal ativado." : "[CIVIA] Modo somente visual ativado.");
    desenharTela();
  }

  if (millis() - ultimaMedicao >= 180) {
    ultimaMedicao = millis();
    distanciaCm = medirDistancia();
    nivelAtual = classificarDistancia(distanciaCm);
    registrarEstado();
  }

  atualizarLeds();
  atualizarSom();

  if (millis() - ultimoDesenho >= 200) {
    ultimoDesenho = millis();
    desenharTela();
  }
}
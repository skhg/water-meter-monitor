/**
 * Copyright 2020 Jack Higgins : https://github.com/skhg
 * All components of this project are licensed under the MIT License.
 * See the LICENSE file for details.
 */

int analogIn = A0;

int multiplexer_s0 = 13;
int multiplexer_s1 = 15;
int multiplexer_s2 = 0;
int multiplexer_s3 = 2;


//  Arduino | NodeMCU | Used
//  0       | D3      | M_S2
//  1       | TX      | X
//  2       | D4      | M_S3
//  3       | RX      | X
//  4       | D2      | BMP180
//  5       | D1      | BMP180
//  6       |
//  7       |
//  8       |
//  9       | S2      | X
//  10      | S3      | X
//  11      |
//  12      | D6      | DHT22
//  13      | D7      | M_S0
//  14      | D5      |
//  15      | D8      | M_S1
//  16      | D0      |

void setup() {
  pinMode(analogIn, INPUT);

  pinMode(multiplexer_s0, OUTPUT);
  pinMode(multiplexer_s1, OUTPUT);
  pinMode(multiplexer_s2, OUTPUT);
  pinMode(multiplexer_s3, OUTPUT);

  digitalWrite(multiplexer_s0, LOW);
  digitalWrite(multiplexer_s1, LOW);
  digitalWrite(multiplexer_s2, LOW);
  digitalWrite(multiplexer_s3, LOW);

  Serial.begin(9600);
}

void loop() {
  Serial.print(readZero());
  Serial.print(",");
  Serial.print(readOne());
  Serial.println();
  delay(100);
}

int readZero() {
  digitalWrite(multiplexer_s0, LOW);
  digitalWrite(multiplexer_s1, LOW);

  return analogRead(analogIn);
}

int readOne() {
  digitalWrite(multiplexer_s0, LOW);
  digitalWrite(multiplexer_s1, HIGH);

  return analogRead(analogIn);
}

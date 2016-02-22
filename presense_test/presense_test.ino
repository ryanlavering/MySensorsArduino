#define DIGITAL_INPUT_SENSOR 5   // The digital input you attached your motion sensor.  (Only 2 and 3 generates interrupt!)

void setup()  
{  
  Serial.begin(115200);
  
  pinMode(DIGITAL_INPUT_SENSOR, INPUT);      // sets the motion sensor digital pin as input
}

void loop()     
{     
  // Read digital motion value
  boolean tripped = (digitalRead(DIGITAL_INPUT_SENSOR) == HIGH); 

  Serial.print("Presense: ");
  if (tripped) {
    Serial.println("I see you!");
  } else {
    Serial.println("Hmm...");
  }
 
  delay(100);
}

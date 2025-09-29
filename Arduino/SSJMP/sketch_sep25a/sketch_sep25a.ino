#include <json_formatting.h>

JsonFormatting objJsonFormatting;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(9600);
  int result = objJsonFormatting.multiply(5,4);
  Serial.print("The result is: ");
  Serial.println(result); // Should print "The result is: 50"
}

void loop() {
  // put your main code here, to run repeatedly:

}

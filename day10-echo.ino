//const int LED_PIN = 10;
const int BUFFER_FULL_LED_PIN = 6;
const int ECHO_LED_PIN = 7;
const int LEARN_LED_PIN = 8;
const int AWAIT_LED_PIN = 9;
const int RECORD_LED_PIN = 10;
const int PLAY_LED_PIN = 11;

const int LED_PINS[] = {ECHO_LED_PIN,LEARN_LED_PIN,AWAIT_LED_PIN,RECORD_LED_PIN,PLAY_LED_PIN,BUFFER_FULL_LED_PIN};

const int SENSOR_PIN = A0;
const int ANALOGUE_MAX = (2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2) - 1 ; /* 10 bit ADC, largest number is 1111111111 binary */
const unsigned long LEARN_MILLIS = 10000;
int max_sensor_value;
int min_sensor_value;
// const int SENSOR_THRESHOLD = ANALOGUE_MAX / 2; - this doesn't work well
int threshold_going_high;
int threshold_going_low;
const int PERCENT_THRESHOLD_OFFSET = 5;
const unsigned long END_OF_MESSAGE_MILLIS = 5000; /* if no change for this long, assume finished */
unsigned long learn_start_millis;

unsigned long last_state_change_millis;

/* If I record times as millis/20, then I can count to 5100 in a uint8_t, making the the array's memory requirement 4 times smaller 
 * Get it working before optimising though 
 */

/* I'm not sure whether I should just assume that a sequence starts with the first 
 * HIGH and finishes with a LOW above a certain length. That seems reasonable, but was not was
 * coding initially. Need to make some changes.
 */

enum SystemState {LEARN,AWAIT,RECORD,PLAY};

//uint8_t initial_lamp_state; // wait until LOW before beginning
const int MAX_LAMP_STATE_CHANGES = 20;
unsigned long lamp_state_change_millis[MAX_LAMP_STATE_CHANGES];
unsigned int lamp_number_state_changes;

uint8_t lamp_state; 
SystemState system_state;

uint8_t get_sensor_state(uint8_t previous_lamp_state) {
  int sensor_value = analogRead(SENSOR_PIN);
  Serial.print("Sensor value = ");
  Serial.println(sensor_value);
  if (previous_lamp_state == HIGH) {
    if (sensor_value < threshold_going_low) {
      return LOW;
    } else {
      return HIGH;
    }
  } else /* previous_lamp_state != HIGH */ {
    if (sensor_value > threshold_going_high) {
      return HIGH;
    } else {
      return LOW;
    }
  }
}

void print_int(String str, int val) {
  Serial.print(str);
  Serial.print(" = ");
  Serial.println(val);
}

void setup() {
  for (int pin = 0; pin < sizeof(LED_PINS); pin++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin,LOW);
  }
  max_sensor_value = analogRead(SENSOR_PIN);
  min_sensor_value = max_sensor_value;

  system_state = LEARN;
  digitalWrite(LEARN_LED_PIN,HIGH);
  Serial.begin(9600);
  Serial.println("Setup done");
  learn_start_millis = millis();
}

unsigned long since_last_change_millis;
int state_changes_played;
uint8_t sensor_state;
uint8_t previous_sensor_state;
int mid_point;

void loop() {
  int sensor_value;
  unsigned long now_millis;
  switch (system_state) {
    case LEARN:
      sensor_value = analogRead(SENSOR_PIN);
      Serial.println("State = LEARN");
      Serial.print("sensor_value = ");
      Serial.println(sensor_value);
      if (sensor_value > max_sensor_value) {
        max_sensor_value = sensor_value;
      }
      if (sensor_value < min_sensor_value) {
        min_sensor_value = sensor_value;
      }
      Serial.print("max_sensor_value = ");
      Serial.print(max_sensor_value);
      Serial.print("; min_sensor_value = ");
      Serial.println(min_sensor_value);
      if (millis() - learn_start_millis > LEARN_MILLIS) {
        int range = max_sensor_value - min_sensor_value;
        mid_point = min_sensor_value + range/2;
        int offset = range * PERCENT_THRESHOLD_OFFSET / 100;
        threshold_going_high = mid_point + offset;
        threshold_going_low = mid_point - offset;
        lamp_state = (sensor_value < mid_point) ? LOW : HIGH;
        if (lamp_state == LOW) { // don't change state until lamp_state goes low
          Serial.print("threshold_going_high = ");
          Serial.println(threshold_going_high);
          Serial.print("threshold_going_low = ");
          Serial.println(threshold_going_low);
          system_state = AWAIT;
          sensor_state = lamp_state;
          digitalWrite(LEARN_LED_PIN,LOW);
          digitalWrite(AWAIT_LED_PIN,HIGH);
          digitalWrite(ECHO_LED_PIN,lamp_state);  
        }
      }
      break;      
    case AWAIT:
      // needs to see a LOW to HIGH transition to continue
      Serial.println("State = AWAIT");
      previous_sensor_state = sensor_state;
      sensor_state = get_sensor_state(lamp_state);
      Serial.println(previous_sensor_state == HIGH ? "HIGH " : "LOW");
      Serial.println(sensor_state == HIGH ? "HIGH " : "LOW");
      if ((previous_sensor_state == LOW) && (sensor_state == HIGH)) {
        lamp_state = sensor_state;
        digitalWrite(ECHO_LED_PIN,lamp_state);
        last_state_change_millis = millis();
        system_state = RECORD;
        lamp_number_state_changes = 0;
        digitalWrite(AWAIT_LED_PIN,LOW);
        digitalWrite(RECORD_LED_PIN,HIGH);
      }
      break;
    case RECORD:
      Serial.println("State = RECORD");
      sensor_state = get_sensor_state(lamp_state);
      Serial.println(sensor_state == HIGH ? "HIGH " : "LOW");
      now_millis = millis();
      since_last_change_millis = millis() - last_state_change_millis;
      if (sensor_state != lamp_state) {
        lamp_state = sensor_state;
        digitalWrite(ECHO_LED_PIN,lamp_state);
        if (lamp_number_state_changes == MAX_LAMP_STATE_CHANGES) { // should never be greater than
          digitalWrite(BUFFER_FULL_LED_PIN,HIGH);
        } else {
          lamp_state_change_millis[lamp_number_state_changes++] = since_last_change_millis;
        }
        last_state_change_millis = now_millis;
      } else if (since_last_change_millis > END_OF_MESSAGE_MILLIS) {
        if (lamp_number_state_changes > 0) {
          system_state = PLAY;
          last_state_change_millis = now_millis;
          state_changes_played = 0;
          lamp_state = HIGH;
          digitalWrite(RECORD_LED_PIN,LOW);
          digitalWrite(ECHO_LED_PIN,HIGH);
          digitalWrite(PLAY_LED_PIN,HIGH);
        } else { // nothing recorded
          system_state = AWAIT;
          lamp_number_state_changes = 0;
          sensor_state = (analogRead(SENSOR_PIN) < mid_point) ? LOW : HIGH;
          Serial.println(sensor_state == HIGH ? "HIGH " : "LOW");
          digitalWrite(RECORD_LED_PIN,LOW);
          digitalWrite(ECHO_LED_PIN,LOW);
          digitalWrite(AWAIT_LED_PIN,HIGH);
        }        
      }
      Serial.print("lamp_number_state_changes =");
      Serial.println(lamp_number_state_changes);
      break;
    case PLAY:
      Serial.println("State = PLAY");
      Serial.println(lamp_state == HIGH ? "HIGH " : "LOW");
      Serial.print("state_changes_played =");
      Serial.println(state_changes_played);
      print_int("lamp_number_state_changes",lamp_number_state_changes);
      now_millis = millis();
      since_last_change_millis = millis() - last_state_change_millis;
      if (since_last_change_millis >= lamp_state_change_millis[state_changes_played]) {
        state_changes_played++;
        last_state_change_millis = now_millis;
        lamp_state = lamp_state == HIGH ? LOW : HIGH;
        digitalWrite(ECHO_LED_PIN,lamp_state);
        if (lamp_number_state_changes == state_changes_played) {
          system_state = AWAIT;
          lamp_number_state_changes = 0;
          sensor_state = (analogRead(SENSOR_PIN) < mid_point) ? LOW : HIGH;
          digitalWrite(BUFFER_FULL_LED_PIN,LOW);
          digitalWrite(PLAY_LED_PIN,LOW);
          digitalWrite(ECHO_LED_PIN,LOW);
          digitalWrite(AWAIT_LED_PIN,HIGH);
        }
      }
      break;
    default:
      Serial.println("State = unknown");
      break;
  }
}

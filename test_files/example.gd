@tool
extends Node2D
class_name HighlightDemo

signal health_changed(current: int, maximum: int)

enum State {
	IDLE,
	RUNNING,
	FINISHED,
}

const MAX_HEALTH: int = 100
const GREETING := "Hello, Godot!"

@export_range(0, MAX_HEALTH, 1) var health: int = MAX_HEALTH
@onready var sprite: Sprite2D = $Sprite2D
@onready var status_label: Label = %StatusLabel

static var instance_count: int = 0

func _ready() -> void:
	instance_count += 1
	print(GREETING)
	print(r"Raw string: \n is kept literally")
	print("A quote inside a string: \"hello\"")

func take_damage(amount: int) -> void:
	if amount <= 0 or health <= 0:
		return

	health = max(health - amount, 0)
	health_changed.emit(health, MAX_HEALTH)
	status_label.text = "HP: %d / %d" % [health, MAX_HEALTH]

	match health:
		0:
			print("Finished")
		_:
			print("Still running")

func calculate_score(base: float, multiplier: float) -> float:
	var result := base * multiplier ** 2
	var ratio := 0.5e+2 + 0x2A + 0b1010 + 0o17
	return result / maxf(ratio, 1.0)

func make_report() -> String:
	return """A multiline string.
It can contain # characters without starting a comment.
The lexer should keep highlighting this text until the closing quotes."""

func build_dictionary() -> Dictionary[String, Variant]:
	return {
		"state": State.IDLE,
		"node": $Sprite2D,
		"unique_node": %StatusLabel,
	}



# TODO: replace this demo with a real scene script.
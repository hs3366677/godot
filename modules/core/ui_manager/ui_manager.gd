class_name UIManager
extends CanvasLayer
## UI Manager Module - Centralized UI management
## Handles screens, popups, transitions, and HUD updates

signal screen_changed(from_screen: String, to_screen: String)
signal popup_opened(popup_name: String)
signal popup_closed(popup_name: String)
signal transition_started
signal transition_finished

enum TransitionType { NONE, FADE, SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN }

## Configuration
@export_group("Transitions")
@export var transition_duration: float = 0.3
@export var default_transition: TransitionType = TransitionType.FADE

@export_group("Screens")
@export var screens_container: Control
@export var popups_container: Control
@export var hud_container: Control
@export var transition_overlay: ColorRect

## Registered screens and popups
var _screens: Dictionary = {}  # name -> Control
var _popups: Dictionary = {}   # name -> Control
var _hud_elements: Dictionary = {}  # name -> Control

## State
var current_screen: String = ""
var _open_popups: Array[String] = []
var _is_transitioning: bool = false

## Singleton
static var instance: UIManager


func _ready() -> void:
	instance = self
	add_to_group("ui_manager")
	_setup_containers()
	_register_existing_ui()


func _setup_containers() -> void:
	# Create containers if not assigned
	if not screens_container:
		screens_container = Control.new()
		screens_container.name = "Screens"
		screens_container.set_anchors_preset(Control.PRESET_FULL_RECT)
		add_child(screens_container)

	if not popups_container:
		popups_container = Control.new()
		popups_container.name = "Popups"
		popups_container.set_anchors_preset(Control.PRESET_FULL_RECT)
		add_child(popups_container)

	if not hud_container:
		hud_container = Control.new()
		hud_container.name = "HUD"
		hud_container.set_anchors_preset(Control.PRESET_FULL_RECT)
		hud_container.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(hud_container)

	if not transition_overlay:
		transition_overlay = ColorRect.new()
		transition_overlay.name = "TransitionOverlay"
		transition_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
		transition_overlay.color = Color.BLACK
		transition_overlay.visible = false
		transition_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(transition_overlay)


func _register_existing_ui() -> void:
	# Register existing children as screens/popups
	for child in screens_container.get_children():
		register_screen(child.name, child)
		child.visible = false

	for child in popups_container.get_children():
		register_popup(child.name, child)
		child.visible = false


## Register a screen
func register_screen(screen_name: String, screen: Control) -> void:
	_screens[screen_name] = screen
	if screen.get_parent() != screens_container:
		screen.reparent(screens_container)


## Register a popup
func register_popup(popup_name: String, popup: Control) -> void:
	_popups[popup_name] = popup
	if popup.get_parent() != popups_container:
		popup.reparent(popups_container)


## Register a HUD element
func register_hud_element(element_name: String, element: Control) -> void:
	_hud_elements[element_name] = element


## Show a screen with optional transition
func show_screen(screen_name: String, transition: TransitionType = TransitionType.FADE) -> void:
	if _is_transitioning:
		return

	if not screen_name in _screens:
		push_error("Screen not found: " + screen_name)
		return

	var previous_screen = current_screen

	if transition == TransitionType.NONE or transition_duration <= 0:
		_switch_screen_immediate(screen_name)
	else:
		await _switch_screen_animated(screen_name, transition)

	screen_changed.emit(previous_screen, screen_name)


func _switch_screen_immediate(screen_name: String) -> void:
	# Hide current screen
	if current_screen in _screens:
		_screens[current_screen].visible = false

	# Show new screen
	_screens[screen_name].visible = true
	current_screen = screen_name


func _switch_screen_animated(screen_name: String, transition: TransitionType) -> void:
	_is_transitioning = true
	transition_started.emit()

	var old_screen = _screens.get(current_screen)
	var new_screen = _screens[screen_name]

	match transition:
		TransitionType.FADE:
			await _transition_fade(old_screen, new_screen)
		TransitionType.SLIDE_LEFT, TransitionType.SLIDE_RIGHT, TransitionType.SLIDE_UP, TransitionType.SLIDE_DOWN:
			await _transition_slide(old_screen, new_screen, transition)

	current_screen = screen_name
	_is_transitioning = false
	transition_finished.emit()


func _transition_fade(old_screen: Control, new_screen: Control) -> void:
	# Fade out
	transition_overlay.visible = true
	transition_overlay.modulate.a = 0.0

	var tween = create_tween()
	tween.tween_property(transition_overlay, "modulate:a", 1.0, transition_duration / 2)
	await tween.finished

	# Switch screens
	if old_screen:
		old_screen.visible = false
	new_screen.visible = true

	# Fade in
	tween = create_tween()
	tween.tween_property(transition_overlay, "modulate:a", 0.0, transition_duration / 2)
	await tween.finished

	transition_overlay.visible = false


func _transition_slide(old_screen: Control, new_screen: Control, direction: TransitionType) -> void:
	var screen_size = get_viewport().get_visible_rect().size
	var slide_offset: Vector2

	match direction:
		TransitionType.SLIDE_LEFT:
			slide_offset = Vector2(-screen_size.x, 0)
		TransitionType.SLIDE_RIGHT:
			slide_offset = Vector2(screen_size.x, 0)
		TransitionType.SLIDE_UP:
			slide_offset = Vector2(0, -screen_size.y)
		TransitionType.SLIDE_DOWN:
			slide_offset = Vector2(0, screen_size.y)

	# Position new screen off-screen
	new_screen.position = -slide_offset
	new_screen.visible = true

	var tween = create_tween()
	tween.set_parallel(true)

	if old_screen:
		tween.tween_property(old_screen, "position", slide_offset, transition_duration)

	tween.tween_property(new_screen, "position", Vector2.ZERO, transition_duration)

	await tween.finished

	if old_screen:
		old_screen.visible = false
		old_screen.position = Vector2.ZERO


## Hide current screen
func hide_screen() -> void:
	if current_screen in _screens:
		_screens[current_screen].visible = false
		current_screen = ""


## Show a popup
func show_popup(popup_name: String, modal: bool = true) -> void:
	if not popup_name in _popups:
		push_error("Popup not found: " + popup_name)
		return

	var popup = _popups[popup_name]
	popup.visible = true

	if modal:
		# Bring to front
		popups_container.move_child(popup, popups_container.get_child_count() - 1)

	if not popup_name in _open_popups:
		_open_popups.append(popup_name)

	popup_opened.emit(popup_name)


## Hide a popup
func hide_popup(popup_name: String) -> void:
	if popup_name in _popups:
		_popups[popup_name].visible = false
		_open_popups.erase(popup_name)
		popup_closed.emit(popup_name)


## Hide all popups
func hide_all_popups() -> void:
	for popup_name in _open_popups.duplicate():
		hide_popup(popup_name)


## Check if any popup is open
func is_popup_open() -> bool:
	return _open_popups.size() > 0


## Check if specific popup is open
func is_popup_visible(popup_name: String) -> bool:
	return popup_name in _open_popups


## Update a HUD element
func update_hud(element_name: String, value: Variant) -> void:
	if not element_name in _hud_elements:
		push_warning("HUD element not found: " + element_name)
		return

	var element = _hud_elements[element_name]

	# Try common update methods
	if element is Label:
		element.text = str(value)
	elif element is ProgressBar:
		element.value = value
	elif element is TextureProgressBar:
		element.value = value
	elif element.has_method("set_value"):
		element.set_value(value)
	elif element.has_method("update_display"):
		element.update_display(value)


## Show/hide HUD
func show_hud() -> void:
	hud_container.visible = true


func hide_hud() -> void:
	hud_container.visible = false


## Get a screen by name
func get_screen(screen_name: String) -> Control:
	return _screens.get(screen_name)


## Get a popup by name
func get_popup(popup_name: String) -> Control:
	return _popups.get(popup_name)


## Get a HUD element by name
func get_hud_element(element_name: String) -> Control:
	return _hud_elements.get(element_name)

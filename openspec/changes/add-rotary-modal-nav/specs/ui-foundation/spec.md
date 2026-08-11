## MODIFIED Requirements

### Requirement: Input Is Delivered To Screens As Intents

Physical input SHALL be translated into device-independent intents before reaching a screen. Screens SHALL respond to intents and SHALL NOT read input hardware directly. The intent type produced by encoder input SHALL depend on application context: when no modal overlay is visible, encoder rotation SHALL produce a volume-change intent and encoder press SHALL produce a transport-toggle intent; when a modal overlay is visible, encoder rotation SHALL produce a navigation intent and encoder press SHALL produce a select intent. Intent routing SHALL be determined before the intent is dispatched to any screen or overlay.

#### Scenario: Rotation becomes a volume intent — no modal open

- **WHEN** the user rotates the encoder and no modal overlay is visible
- **THEN** a volume-change intent carrying the delta is delivered to the active screen

#### Scenario: Rotation becomes a navigation intent — modal open

- **WHEN** the user rotates the encoder and a modal overlay is visible
- **THEN** a navigation intent is delivered to the active modal overlay
- **AND** no volume-change intent is produced

#### Scenario: Encoder press becomes a select intent — modal open

- **WHEN** the user presses the encoder button and a modal overlay is visible
- **THEN** a select intent is delivered to the active modal overlay
- **AND** no transport-toggle intent is produced

#### Scenario: Screens do not read hardware

- **WHEN** a screen handles user input
- **THEN** it reads only intents
- **AND** it does not reference an input device directly

#### Scenario: Input during a screen transition

- **WHEN** an intent arrives while a screen transition is in progress
- **THEN** the intent is either delivered to the incoming screen or discarded
- **AND** it is never delivered to a screen that has been torn down

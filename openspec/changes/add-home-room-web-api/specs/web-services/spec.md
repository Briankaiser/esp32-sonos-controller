## Purpose

Extends the on-device HTTP server with two room-management endpoints that allow an operator to inspect discovered Sonos speakers and configure the home room without physical access.

## ADDED Requirements

### Requirement: Device Exposes Discovered Speaker List via HTTP

The device SHALL expose `GET /rooms` on port 80. The response SHALL be a JSON array where each element represents one discovered Sonos speaker. Each element SHALL include at minimum the speaker's UUID and its human-readable friendly name. When no speakers have been discovered yet, the response SHALL be an empty JSON array. The endpoint SHALL require no authentication.

#### Scenario: Speakers have been discovered

- **WHEN** a client issues `GET /rooms` and one or more Sonos speakers have been discovered
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `application/json`
- **AND** the body is a JSON array with one object per speaker containing `"uuid"` and `"name"` fields

#### Scenario: No speakers discovered yet

- **WHEN** a client issues `GET /rooms` and no discovery cycle has completed or no speakers were found
- **THEN** the device responds with HTTP 200
- **AND** the body is an empty JSON array `[]`

### Requirement: Device Accepts Home Room Selection via HTTP

The device SHALL expose `POST /rooms/select` on port 80. The endpoint SHALL accept a `uuid` parameter in either a URL-encoded form body or as a query parameter. If the UUID is non-empty, the device SHALL write it to NVS as the home room UUID. The next discovery cycle SHALL attempt to resolve the stored UUID against the speaker inventory. The endpoint SHALL require no authentication. If the UUID is empty or the parameter is absent, the device SHALL respond with HTTP 400.

#### Scenario: Valid UUID is stored as home room

- **WHEN** a client sends `POST /rooms/select` with a non-empty `uuid` parameter
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `application/json`
- **AND** the response body confirms the stored UUID
- **AND** the next discovery cycle resolves the home room if the UUID matches a discovered speaker

#### Scenario: Missing or empty UUID returns 400

- **WHEN** a client sends `POST /rooms/select` with an empty or absent `uuid` parameter
- **THEN** the device responds with HTTP 400
- **AND** the NVS home room value is unchanged

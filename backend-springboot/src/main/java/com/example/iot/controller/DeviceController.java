package com.example.iot.controller;

import com.example.iot.dto.TelemetryAverageResponse;
import com.example.iot.dto.DeviceSettingRequest;
import com.example.iot.dto.DeviceSettingResponse;
import com.example.iot.entity.Command;
import com.example.iot.entity.Device;
import com.example.iot.entity.Telemetry;
import com.example.iot.service.CommandService;
import com.example.iot.service.DeviceService;
import com.example.iot.service.TelemetryService;
import com.example.iot.service.DeviceSettingService;
import jakarta.validation.Valid;
import lombok.Data;
import lombok.RequiredArgsConstructor;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.format.annotation.DateTimeFormat;
import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.web.bind.annotation.*;

import java.time.ZonedDateTime;
import java.util.List;

@RestController
@RequestMapping("/api/v1")
@RequiredArgsConstructor
public class DeviceController {

    private final DeviceService deviceService;
    private final TelemetryService telemetryService;
    private final CommandService commandService;
    private final DeviceSettingService deviceSettingService;

    // ============================================================
    // DEVICE
    // ============================================================

    @GetMapping("/devices")
    public ResponseEntity<List<Device>> getAllDevices() {

        return ResponseEntity.ok(
                deviceService.getAllDevices()
        );
    }

    @GetMapping("/devices/{deviceId}")
    public ResponseEntity<Device> getDevice(
            @PathVariable String deviceId) {

        return ResponseEntity.ok(
                deviceService.getDeviceByDeviceId(deviceId)
        );
    }

    // ============================================================
    // DEVICE SETTINGS
    // ============================================================

    @GetMapping("/devices/{deviceId}/settings")
    public ResponseEntity<DeviceSettingResponse> getDeviceSettings(
            @PathVariable String deviceId) {
        return ResponseEntity.ok(deviceSettingService.getSettings(deviceId));
    }

    @PutMapping("/devices/{deviceId}/settings")
    @PreAuthorize("hasAnyRole('ADMIN', 'OPERATOR')")
    public ResponseEntity<DeviceSettingResponse> updateDeviceSettings(
            @PathVariable String deviceId,
            @Valid @RequestBody DeviceSettingRequest request) {
        return ResponseEntity.ok(deviceSettingService.updateSettings(deviceId, request));
    }

    // ============================================================
    // TELEMETRY MỚI NHẤT
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry/latest")
    public ResponseEntity<Telemetry> getLatestTelemetry(
            @PathVariable String deviceId) {

        Telemetry telemetry =
                telemetryService.getLatestTelemetry(deviceId);

        if (telemetry == null) {
            return ResponseEntity.notFound().build();
        }

        return ResponseEntity.ok(telemetry);
    }

    // ============================================================
    // TELEMETRY HISTORY
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry")
    public ResponseEntity<Page<Telemetry>> getTelemetryHistory(
            @PathVariable String deviceId,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime from,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime to,

            @RequestParam(defaultValue = "0")
            int page,

            @RequestParam(defaultValue = "20")
            int size) {

        return ResponseEntity.ok(
                telemetryService.getTelemetryHistory(
                        deviceId,
                        from,
                        to,
                        PageRequest.of(page, size)
                )
        );
    }

    // ============================================================
    // TRUNG BÌNH THEO GIỜ
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry/average/hourly")
    public ResponseEntity<List<TelemetryAverageResponse>>
    getHourlyAverage(
            @PathVariable String deviceId,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime from,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime to) {

        if (from == null && to == null) {

            to = ZonedDateTime.now();
            from = to.minusHours(24);

        } else if (from == null || to == null) {

            return ResponseEntity.badRequest().build();
        }

        if (from.isAfter(to)) {
            return ResponseEntity.badRequest().build();
        }

        return ResponseEntity.ok(
                telemetryService.getHourlyAverages(
                        deviceId,
                        from,
                        to
                )
        );
    }

    // ============================================================
    // TRUNG BÌNH THEO NGÀY
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry/average/daily")
    public ResponseEntity<List<TelemetryAverageResponse>>
    getDailyAverage(
            @PathVariable String deviceId,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime from,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime to) {

        if (from == null && to == null) {

            to = ZonedDateTime.now();
            from = to.minusDays(7);

        } else if (from == null || to == null) {

            return ResponseEntity.badRequest().build();
        }

        if (from.isAfter(to)) {
            return ResponseEntity.badRequest().build();
        }

        return ResponseEntity.ok(
                telemetryService.getDailyAverages(
                        deviceId,
                        from,
                        to
                )
        );
    }

    // ============================================================
    // TRUNG BÌNH THEO THÁNG
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry/average/monthly")
    public ResponseEntity<List<TelemetryAverageResponse>>
    getMonthlyAverage(
            @PathVariable String deviceId,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime from,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime to) {

        // Mặc định lấy 12 tháng gần nhất
        if (from == null && to == null) {

            to = ZonedDateTime.now();
            from = to.minusMonths(12);

        } else if (from == null || to == null) {

            return ResponseEntity.badRequest().build();
        }

        if (from.isAfter(to)) {
            return ResponseEntity.badRequest().build();
        }

        return ResponseEntity.ok(
                telemetryService.getMonthlyAverages(
                        deviceId,
                        from,
                        to
                )
        );
    }

    // ============================================================
    // TRUNG BÌNH THEO NĂM
    // ============================================================

    @GetMapping("/devices/{deviceId}/telemetry/average/yearly")
    public ResponseEntity<List<TelemetryAverageResponse>>
    getYearlyAverage(
            @PathVariable String deviceId,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime from,

            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME)
            ZonedDateTime to) {

        // Mặc định lấy 5 năm gần nhất
        if (from == null && to == null) {

            to = ZonedDateTime.now();
            from = to.minusYears(5);

        } else if (from == null || to == null) {

            return ResponseEntity.badRequest().build();
        }

        if (from.isAfter(to)) {
            return ResponseEntity.badRequest().build();
        }

        return ResponseEntity.ok(
                telemetryService.getYearlyAverages(
                        deviceId,
                        from,
                        to
                )
        );
    }

    // ============================================================
    // COMMAND HISTORY
    // ============================================================

    @GetMapping("/devices/{deviceId}/commands")
    public ResponseEntity<Page<Command>> getCommandHistory(
            @PathVariable String deviceId,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "10") int size) {

        return ResponseEntity.ok(
                commandService.getCommandHistory(
                        deviceId,
                        PageRequest.of(page, size)
                )
        );
    }

    // ============================================================
    // SEND COMMAND
    // ============================================================

    @PostMapping("/devices/{deviceId}/commands")
    @PreAuthorize("hasAnyRole('ADMIN', 'OPERATOR')")
    public ResponseEntity<Command> sendCommand(
            @PathVariable String deviceId,
            @RequestBody CommandRequest request) {

        Command command =
                commandService.sendCommand(
                        deviceId,
                        request.getAction()
                );

        return ResponseEntity.ok(command);
    }
}

@Data
class CommandRequest {

    private String action;
}

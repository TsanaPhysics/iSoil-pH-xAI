<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/post_data.php
 * Description: REST API for ingesting measurement data from Wio Terminal / Serial Bridge
 */

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

require_once __DIR__ . '/db.php';

// รองรับทั้ง JSON Body และ URL Encoded Form
$rawInput = file_get_contents('php://input');
$data = json_decode($rawInput, true);

if (!$data) {
    $data = $_POST;
}

// ตรวจสอบค่าพารามิเตอร์ที่จำเป็น
if (!isset($data['voltage']) || !isset($data['ph_ai'])) {
    http_response_code(400);
    echo json_encode([
        'success' => false,
        'error' => 'Missing required fields: voltage and ph_ai'
    ]);
    exit;
}

$voltage = floatval($data['voltage']);
$temp_c = isset($data['temp_c']) ? floatval($data['temp_c']) : 25.0;
$ph_ai = floatval($data['ph_ai']);
$ph_traditional = isset($data['ph_traditional']) ? floatval($data['ph_traditional']) : $ph_ai;
$target_buffer = isset($data['target_buffer']) ? trim($data['target_buffer']) : 'FIELD';
$sample_note = isset($data['sample_note']) ? trim($data['sample_note']) : '';

$delta_error = round($ph_traditional - $ph_ai, 3);

// วินิจฉัยสภาพดิน
$soil_status = 'NORMAL';
if ($ph_ai < 5.0) {
    $soil_status = 'CRITICAL: VERY ACIDIC';
} elseif ($ph_ai >= 5.5 && $ph_ai <= 6.5) {
    $soil_status = 'OPTIMAL DURIAN SOIL';
} elseif ($ph_ai > 6.5 && $ph_ai <= 7.5) {
    $soil_status = 'NEUTRAL SOIL';
} elseif ($ph_ai > 7.5) {
    $soil_status = 'ALKALINE SOIL';
} else {
    $soil_status = 'SLIGHTLY ACIDIC (5.0 - 5.5)';
}

$timestamp = isset($data['datetime']) && !empty($data['datetime']) 
    ? trim($data['datetime']) 
    : date('Y-m-d H:i:s');

$session_id = isset($data['session_id']) && !empty($data['session_id']) 
    ? trim($data['session_id']) 
    : 'EXP_001';

$model_name = isset($data['model_name']) && !empty($data['model_name']) 
    ? trim($data['model_name']) 
    : 'ANN DURIAN';

$mode = isset($data['mode']) && !empty($data['mode']) 
    ? trim($data['mode']) 
    : 'FIELD_RUN';

$confidence = isset($data['confidence']) ? floatval($data['confidence']) : 98.6;
$temp_mode = isset($data['temp_mode']) && !empty($data['temp_mode']) ? trim($data['temp_mode']) : 'MTC';

try {
    $stmt = $db->prepare("INSERT INTO measurements 
        (session_id, timestamp, voltage, temp_c, ph_traditional, ph_ai, delta_error, target_buffer, soil_status, sample_note, model_name, mode, confidence, temp_mode) 
        VALUES (:session_id, :timestamp, :voltage, :temp_c, :ph_traditional, :ph_ai, :delta_error, :target_buffer, :soil_status, :sample_note, :model_name, :mode, :confidence, :temp_mode)");

    $stmt->execute([
        ':session_id' => $session_id,
        ':timestamp' => $timestamp,
        ':voltage' => $voltage,
        ':temp_c' => $temp_c,
        ':ph_traditional' => $ph_traditional,
        ':ph_ai' => $ph_ai,
        ':delta_error' => $delta_error,
        ':target_buffer' => $target_buffer,
        ':soil_status' => $soil_status,
        ':sample_note' => $sample_note,
        ':model_name' => $model_name,
        ':mode' => $mode,
        ':confidence' => $confidence,
        ':temp_mode' => $temp_mode
    ]);

    $insertId = $db->lastInsertId();

    echo json_encode([
        'success' => true,
        'id' => $insertId,
        'message' => 'Data recorded successfully',
        'data' => [
            'ph_ai' => $ph_ai,
            'ph_traditional' => $ph_traditional,
            'voltage' => $voltage,
            'temp_c' => $temp_c,
            'soil_status' => $soil_status
        ]
    ]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'error' => 'Insert failed: ' . $e->getMessage()
    ]);
}

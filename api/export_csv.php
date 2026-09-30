<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/export_csv.php
 * Description: Export entire database records to downloadable CSV
 */

require_once __DIR__ . '/db.php';

try {
    $stmt = $db->query("SELECT id, timestamp, voltage, temp_c, ph_traditional, ph_ai, delta_error, target_buffer, soil_status, sample_note FROM measurements ORDER BY id ASC");
    
    $filename = "soil_ph_dataset_" . date('Ymd_His') . ".csv";
    
    header('Content-Type: text/csv; charset=utf-8');
    header('Content-Disposition: attachment; filename=' . $filename);
    
    $output = fopen('php://output', 'w');
    
    // เขียน BOM สำหรับ UTF-8 ให้เปิดใน Excel ได้ถูกต้อง
    fprintf($output, chr(0xEF).chr(0xBB).chr(0xBF));
    
    // เขียนหัวคอลัมน์
    fputcsv($output, [
        'Record_ID',
        'Timestamp',
        'Cell_Potential_V',
        'Temperature_C',
        'pH_Traditional_Nernst',
        'pH_AI_Predicted',
        'Delta_Compensation_pH',
        'Target_Buffer',
        'Agronomy_Status',
        'Sample_Note'
    ]);
    
    while ($row = $stmt->fetch()) {
        fputcsv($output, $row);
    }
    
    fclose($output);
    exit;
} catch (PDOException $e) {
    header('Content-Type: text/plain; charset=utf-8');
    echo "Error exporting CSV: " . $e->getMessage();
}

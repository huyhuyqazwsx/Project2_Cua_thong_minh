package com.example.smart_door

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import java.util.UUID

class DoiMatKhauWifi : AppCompatActivity() {

    private val REQUEST_ENABLE_BT = 1001
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private var bluetoothGatt: BluetoothGatt? = null
    private var ssidChar: BluetoothGattCharacteristic? = null
    private var passChar: BluetoothGattCharacteristic? = null
    private lateinit var MAC : String
    private lateinit var PASSWORD_UUID: UUID
    private lateinit var SERVICE_UUID: UUID
    private lateinit var SSID_UUID: UUID
    private lateinit var btnSend : Button
    private lateinit var btnQuayLai1 : Button
    private lateinit var edtSsid : EditText
    private lateinit var edtPassword : EditText
    private lateinit var statusText : TextView
    private var isFound : Boolean = false

    @SuppressLint("MissingPermission")
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_doi_mat_khau_wifi)
        val infoUUID = intent.getSerializableExtra("infoUUID") as? DoorController.InfoUUID

        if (infoUUID != null) {
            Toast.makeText(this@DoiMatKhauWifi, "Da lay thong tin", Toast.LENGTH_SHORT).show()

            MAC = infoUUID.MAC
            PASSWORD_UUID = UUID.fromString(infoUUID.PASSWORD_UUID)
            SERVICE_UUID = UUID.fromString(infoUUID.SERVICE_UUID)
            SSID_UUID = UUID.fromString(infoUUID.SSID_UUID)
        }
        else{
            Toast.makeText(this@DoiMatKhauWifi, "Khong co thong tin", Toast.LENGTH_SHORT).show()
            startActivity(Intent(this@DoiMatKhauWifi, DoorController::class.java))
            finish()
        }

        btnSend = findViewById(R.id.btnSend)
        btnQuayLai1 = findViewById(R.id.btnQuayLai1)
        edtPassword = findViewById(R.id.edtPassword)
        edtSsid = findViewById(R.id.edtSsid)
        statusText = findViewById(R.id.statusText)

        ActivityCompat.requestPermissions(this, arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        ), 1)

        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (!bluetoothAdapter.isEnabled) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
        }

        scanBLE()


        btnSend.setOnClickListener{
            val ssid = edtSsid.text.toString()
            val password = edtPassword.text.toString()

            sendToWifi(ssid, password)
        }

        btnQuayLai1.setOnClickListener{
            startActivity(Intent(this@DoiMatKhauWifi, DoorController::class.java))
            finish()
        }

    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    @SuppressLint("MissingPermission")
    private fun sendToWifi(ssid: String, password: String) {
        if(!isFound){
            Toast.makeText(this@DoiMatKhauWifi, "Dang tim thiet bi", Toast.LENGTH_SHORT).show()
            return
        }
        if(ssid.isEmpty() || password.isEmpty()){
            Toast.makeText(this@DoiMatKhauWifi, "Thieu thong tin", Toast.LENGTH_SHORT).show()
        }
        else{
            //gui thong tin
            bluetoothGatt?.writeCharacteristic(ssidChar!!, ssid.toByteArray(), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)

            Handler(Looper.getMainLooper()).postDelayed({
                bluetoothGatt?.writeCharacteristic(passChar!!, password.toByteArray(), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                statusText.text = "Đã gửi"
            }, 300)
        }
    }

    @SuppressLint("MissingPermission")
    private fun scanBLE(){
        statusText.text = "Scanning..."

        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid.fromString(SERVICE_UUID.toString()))
            .build()

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bluetoothAdapter.bluetoothLeScanner.startScan(listOf(filter), settings, object : ScanCallback(){
            override fun onScanResult(callbackType: Int, result: ScanResult) {

                statusText.text = "ESP32 found"
                bluetoothAdapter.bluetoothLeScanner.stopScan(this)
                result.device.connectGatt(this@DoiMatKhauWifi, false , gattCallback)

            }

            override fun onScanFailed(errorCode: Int) {
                Toast.makeText(this@DoiMatKhauWifi, "Loi khi quet", Toast.LENGTH_SHORT).show()
                statusText.text = "Failed"
            }
        })
    }

    private val gattCallback = object : BluetoothGattCallback(){
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            //neu ket noi thanh cong
            if(newState == BluetoothProfile.STATE_CONNECTED){
                bluetoothGatt = gatt
                gatt.discoverServices()
                statusText.text = "Kết nối thành công"
            }
            else if(newState == BluetoothProfile.STATE_DISCONNECTED){
                statusText.text = "Đã ngắt kết nối"
                isFound= false
                scanBLE()
                gatt.close()
            }
            else{
                statusText.text = "Lỗi kết nối"
                isFound= false
                scanBLE()
                gatt.close()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            gatt.getService(SERVICE_UUID)?.let{ service ->
                ssidChar = service.getCharacteristic(SSID_UUID)
                passChar = service.getCharacteristic(PASSWORD_UUID)
                statusText.text = "Sẵn sàng gửi"
                isFound = true
            }
        }
    }
}
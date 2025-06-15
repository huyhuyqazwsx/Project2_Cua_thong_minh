package com.example.smart_door

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.FirebaseUser
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import kotlinx.coroutines.Runnable
import java.io.Serializable

@SuppressLint("UseSwitchCompatOrMaterialCode")
class DoorController : AppCompatActivity() {

    private lateinit var database : DatabaseReference
    private lateinit var auth : FirebaseAuth
    private lateinit var btnLogout : Button
    private lateinit var btnDoiWifi : Button
    private lateinit var txtDoorStatus : TextView
    private lateinit var swtDoor : Switch
    private lateinit var txtConnectStatus : TextView
    private var currentIdEsp32: String? = null
    private var connectionCheckHandel : Handler? = null
    private var user : FirebaseUser? = null
    private var lastTimeEsp32 : Long? = null
    private var isOnline: Boolean = false
    private var isOpen: Boolean = false
    private var isLock : Boolean = false
    private var countConnect : Int = 1

    data class InfoUUID(
        val MAC: String = "",
        val PASSWORD_UUID: String = "",
        val SERVICE_UUID: String = "",
        val SSID_UUID: String = ""
    ): Serializable

    private var infoUUID : InfoUUID? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_door_controller)

        // Khoi tao ui
        auth = FirebaseAuth.getInstance()
        database = FirebaseDatabase.getInstance().reference

        btnLogout = findViewById(R.id.btnLogout)
        btnDoiWifi = findViewById(R.id.btnDoiWifi)
        txtDoorStatus = findViewById(R.id.txtStatusDoor)
        swtDoor = findViewById(R.id.swtDoor)
        txtConnectStatus = findViewById(R.id.txtConnectStatus)

        // Kiem tra dang nhap
        user = auth.currentUser
        if(user == null){
            Toast.makeText(this@DoorController, "Đăng xuất", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        //Khoi tao 1 handle moi gan cho luong main
        connectionCheckHandel = Handler(Looper.getMainLooper())

        getIdEsp32();

        addListenerForButton()

    }

    private fun addListenerForButton(){
        //gan su kien nghe khi khoa
        swtDoor.setOnCheckedChangeListener { _, isLock ->
            //Neu dang dong cua
            if (!isOpen){
                updateLockFirebase(isLock)
            }

            //Neu dang mo cua
            else{
                swtDoor.isChecked = false
                Toast.makeText(this@DoorController, "Cửa đang mở", Toast.LENGTH_SHORT).show()
            }
        }

        //Gan su kien logout
        btnLogout.setOnClickListener{
            auth.signOut()
            startActivity(Intent(this@DoorController, DangNhap::class.java))
            finish()
        }

        //Gan su kien doi mat khau wifi
        btnDoiWifi.setOnClickListener{
            val intent = Intent(this@DoorController, DoiMatKhauWifi::class.java)
            intent.putExtra("infoUUID", infoUUID)  // truyền object
            startActivity(intent)
        }

    }

    private fun getIdEsp32() {
        user?.let {
            database.child("users").child(it.uid).child("idEsp32").get()
                .addOnSuccessListener { snapshot ->
                    currentIdEsp32 = snapshot.getValue(String::class.java)
                    if(currentIdEsp32 != null){
                        Toast.makeText(this@DoorController, "Tìm thấy id thiết bị", Toast.LENGTH_SHORT).show()
                        getInfoUUID()
                    } else{
                        Toast.makeText(this@DoorController, "Không tìm thấy ID thiết bị", Toast.LENGTH_SHORT).show()
                        finish()
                    }
                }

                .addOnFailureListener {
                    Toast.makeText(this@DoorController, "Lỗi lấy ID thiết bị", Toast.LENGTH_SHORT).show()
                    finish()
                }
        }
    }

    private fun getInfoUUID(){

        currentIdEsp32?.let {
            database.child("doors").child(it).child("infoUUID").get()
                .addOnSuccessListener { snapshot ->
                    infoUUID = snapshot.getValue(InfoUUID::class.java)
                    if(infoUUID != null){
                        Toast.makeText(this@DoorController, "Lấy thông tin BLE thành công", Toast.LENGTH_SHORT).show()
                        //Sau khi co day du thong tin
                        initConnect()
                        monitorConnect()
                        startConnectionCheck()
                        monitorDoor()
                    }
                    else{
                        Toast.makeText(this@DoorController, "Không tìm thấy thong tin BLE", Toast.LENGTH_SHORT).show()
                        finish()
                    }
                }

                .addOnFailureListener {
                    Toast.makeText(this@DoorController, "Lỗi khi lấy thông tin BLE", Toast.LENGTH_SHORT).show()
                }

        }
    }


    private fun initConnect() {
        //Cap nhat lai isOpen
        currentIdEsp32?.let {
            database.child("doors").child(it).child("isOpen").get()
                .addOnSuccessListener { snapshot ->
                    isOpen = snapshot.getValue(Boolean::class.java) == true
                    if(isOpen){
                        txtDoorStatus.text = "Cửa đang mở"
                    } else{
                        txtDoorStatus.text = "Cửa đang đóng"
                    }
                }
                .addOnFailureListener {
                    swtDoor.isChecked = false
                }
        }

        //Cap nhat isLock
        currentIdEsp32?.let {
            database.child("doors").child(it).child("isLock").get()
                .addOnSuccessListener { snapshot ->
                    isLock = snapshot.getValue(Boolean::class.java) == true
                    if(isLock){
                        swtDoor.isChecked = true
                    } else{
                        swtDoor.isChecked = false
                    }
                }
                .addOnFailureListener {
                    txtDoorStatus.text = "Đang tải..."
                }
        }
    }

    private fun updateLockFirebase(isLock: Boolean) {
        //Neu offline
        if(txtConnectStatus.text == "Offline"){
            swtDoor.isChecked = !isLock
            Toast.makeText(this@DoorController, "Thiết bị đang offline", Toast.LENGTH_SHORT).show()
            return
        }

        //Neu khoa
        if(isLock){
            currentIdEsp32?.let {
                database.child("doors").child(it).child("isLock").setValue(true)
                    .addOnSuccessListener {
                        Toast.makeText(this@DoorController, "Cập nhật trạng thái thành công", Toast.LENGTH_SHORT).show()
                    }

                    .addOnFailureListener {
                        swtDoor.isChecked = false
                        Toast.makeText(this@DoorController, "Lỗi khóa cửa", Toast.LENGTH_SHORT).show()
                    }
            }
        }
        //Neu mo khoa
        else{
            currentIdEsp32?.let {
                database.child("doors").child(it).child("isLock").setValue(false)
                    .addOnSuccessListener {
                        Toast.makeText(this@DoorController, "Cập nhật trạng thái thành công", Toast.LENGTH_SHORT).show()
                    }

                    .addOnFailureListener {
                        swtDoor.isChecked = true
                        Toast.makeText(this@DoorController, "Lỗi mở khóa cửa", Toast.LENGTH_SHORT).show()
                    }
            }
        }
    }

    private fun monitorDoor() {
        //Lang nghe o isOpen
        currentIdEsp32?.let {
            database.child("doors").child(it).child("isOpen")
                .addValueEventListener(object : ValueEventListener{
                    override fun onDataChange(snapshot: DataSnapshot) {
                        isOpen = snapshot.getValue(Boolean::class.java) ?: false
                        if(isOpen){
                            txtDoorStatus.text = "Cửa đang mở"
                        } else{
                            txtDoorStatus.text = "Cửa đang đóng"
                        }
                    }

                    override fun onCancelled(error: DatabaseError) {
                        txtDoorStatus.text = "Đang tải..."
                        Toast.makeText(this@DoorController, "Lỗi lấy thông tin isOpen", Toast.LENGTH_SHORT).show()
                    }

                })
        }

        //Lang nghe o isLock
        currentIdEsp32?.let {
            database.child("doors").child(it).child("isLock")
                .addValueEventListener(object : ValueEventListener{
                    override fun onDataChange(snapshot: DataSnapshot) {
                        isLock = snapshot.getValue(Boolean::class.java) ?: false
                        if(isLock){
                            swtDoor.isChecked = true
                        } else{
                            swtDoor.isChecked = false
                        }
                    }

                    override fun onCancelled(error: DatabaseError) {
                        swtDoor.isChecked = false
                        Toast.makeText(this@DoorController, "Lỗi lấy thông tin isLock", Toast.LENGTH_SHORT).show()
                    }
                })
        }
    }

    private fun monitorConnect(){
        //Theo doi time
        currentIdEsp32?.let {
            database.child("doors").child(it).child("lastTime")
                .addValueEventListener(object : ValueEventListener{
                    override fun onDataChange(snapshot: DataSnapshot) {
                        lastTimeEsp32 = snapshot.getValue(Long::class.java)
                        isOnline = true
                        txtConnectStatus.text = "Online"

                        if(countConnect == 1){
                            countConnect++
                            isOnline = false
                            txtConnectStatus.text = "Offline"
                        }

                    }

                    override fun onCancelled(error: DatabaseError) {
                        txtConnectStatus.text = "Offline"
                        Toast.makeText(this@DoorController, "Lỗi thao tác database" , Toast.LENGTH_SHORT).show()
                    }
                })
        }
    }

    //Khoi tao ket noi
    private fun startConnectionCheck() {
        //Kiem tra dinh ky
        connectionCheckHandel?.post(object : Runnable{
            override fun run() {
                checkOnline()
                //Gui lai yeu cầu mỗi 3 giay
                connectionCheckHandel?.postDelayed(this, 3000)
            }
        })
    }



    private fun checkOnline() {
        if(lastTimeEsp32 == null){
            lastTimeEsp32 = 0
        }

        if(isOnline){
            //Toast.makeText(this@DoorController, "Online", Toast.LENGTH_SHORT).show()
            txtConnectStatus.text = "Online"
        }
        else{
            //Toast.makeText(this@DoorController, "Offline", Toast.LENGTH_SHORT).show()
            swtDoor.isChecked = false
            txtConnectStatus.text = "Offline"
        }

        isOnline = false

    }
}
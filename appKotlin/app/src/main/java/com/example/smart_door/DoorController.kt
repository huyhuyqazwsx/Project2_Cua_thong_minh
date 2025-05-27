package com.example.smart_door

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import kotlinx.coroutines.delay

@SuppressLint("UseSwitchCompatOrMaterialCode")
class DoorController : AppCompatActivity() {

    private lateinit var database : DatabaseReference
    private lateinit var auth : FirebaseAuth
    private lateinit var btnLogout : Button
    private lateinit var txtDoorStatus : TextView
    private lateinit var swtDoor : Switch
    private var currentIdEsp32: String? = null
    private var doorListener: ValueEventListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_door_controller)

        // Khoi tao ui
        auth = FirebaseAuth.getInstance()
        database = FirebaseDatabase.getInstance().reference

        btnLogout = findViewById(R.id.btnLogout)
        txtDoorStatus = findViewById(R.id.txtStatusDoor)
        swtDoor = findViewById(R.id.swtDoor)

        // Kiem tra dang nhap
        val currentUser = auth.currentUser
        if(currentUser == null){
            Toast.makeText(this@DoorController, "Đăng xuất", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        // Lay ID ESP32
        getUserEsp32Id()
    }

    private fun getUserEsp32Id() {
        val user = auth.currentUser ?: return

        database.child("users").child(user.uid).child("idEsp32").get()
            .addOnSuccessListener { snapshot ->
                currentIdEsp32 = snapshot.getValue(String::class.java)
                if (currentIdEsp32 != null) {
                    // Sau khi co ID, thuc hien cac thao tac khac
                    readDoorStatus()
                    setupListener()
                } else {
                    Toast.makeText(this@DoorController, "Không tìm thấy ID thiết bị", Toast.LENGTH_SHORT).show()
                    finish()
                }
            }
            .addOnFailureListener { exception ->
                Toast.makeText(this@DoorController, "Lỗi lấy thông tin thiết bị: ${exception.message}", Toast.LENGTH_SHORT).show()
                finish()
            }
    }

    private fun readDoorStatus() {
        val idEsp32 = currentIdEsp32 ?: return

        database.child("doors").child(idEsp32).child("status").get()
            .addOnSuccessListener { snapshot ->
                val status = snapshot.getValue(Boolean::class.java) ?: false
                updateUI(status)
            }
            .addOnFailureListener { exception ->
                Toast.makeText(this@DoorController, "Không thể đọc trạng thái cửa: ${exception.message}", Toast.LENGTH_SHORT).show()
            }
    }

    private fun updateUI(status: Boolean) {
        runOnUiThread {
            swtDoor.setOnCheckedChangeListener(null)
            swtDoor.isChecked = status

            txtDoorStatus.text = if (status) {
                "Cửa Đang Mở"
            } else {
                "Cửa Đang Đóng"
            }

            txtDoorStatus.setTextColor(
                if (status) {
                    resources.getColor(android.R.color.holo_red_light, null)
                } else {
                    resources.getColor(android.R.color.holo_blue_light, null)
                }
            )

            // Gan lai listener
            swtDoor.setOnCheckedChangeListener { _, isChecked ->
                updateDoorStatus(isChecked)
            }
        }
    }

    private fun setupListener() {
        val idEsp32 = currentIdEsp32 ?: return

        btnLogout.setOnClickListener{
            updateDoorStatus(false)
            logOut()
        }

        // Realtime listener cho door status
        doorListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val status = snapshot.child("status").getValue(Boolean::class.java) ?: false
                updateUI(status)
                Log.d("Firebase", "Door status updated via listener: $status")
            }

            override fun onCancelled(error: DatabaseError) {
                Log.e("Firebase", "Realtime listener cancelled", error.toException())
                Toast.makeText(this@DoorController, "Mất kết nối realtime: ${error.message}", Toast.LENGTH_SHORT).show()
            }
        }

        database.child("doors").child(idEsp32).addValueEventListener(doorListener!!)
    }

    private fun updateDoorStatus(isChecked: Boolean) {
        val user = auth.currentUser
        val idEsp32 = currentIdEsp32

        if(user == null){
            Toast.makeText(this@DoorController, "Bạn cần đăng nhập để điều khiển cửa", Toast.LENGTH_SHORT).show()
            return
        }

        if(idEsp32 == null) {
            Toast.makeText(this@DoorController, "Không tìm thấy ID thiết bị", Toast.LENGTH_SHORT).show()
            return
        }

        // Neu mo cua
        if(isChecked){
            database.child("doors").child(idEsp32).child("status").setValue(true)
                .addOnSuccessListener {
                    Toast.makeText(this@DoorController, "Cửa đã mở", Toast.LENGTH_SHORT).show()

                    // Tu dong dong sau 5 giay
                    Handler(Looper.getMainLooper()).postDelayed({
                        database.child("doors").child(idEsp32).child("status").setValue(false)
                            .addOnSuccessListener {
                                Log.d("Firebase", "Door auto-closed after 5 seconds")
                                Toast.makeText(this@DoorController, "Cửa tự động đóng", Toast.LENGTH_SHORT).show()
                            }
                            .addOnFailureListener { exception ->
                                Log.e("Firebase", "Failed to auto-close door", exception)
                                Toast.makeText(this@DoorController, "Lỗi tự động đóng cửa: ${exception.message}", Toast.LENGTH_SHORT).show()
                            }
                    }, 5000)

                }
                .addOnFailureListener { exception ->
                    Toast.makeText(this@DoorController, "Lỗi khi mở cửa: ${exception.message}", Toast.LENGTH_SHORT).show()
                    swtDoor.isChecked = false
                }
        }
        // Neu dong cua
        else{
            database.child("doors").child(idEsp32).child("status").setValue(false)
                .addOnSuccessListener {
                    Toast.makeText(this@DoorController, "Cửa đã đóng", Toast.LENGTH_SHORT).show()
                }
                .addOnFailureListener{ exception ->
                    Toast.makeText(this@DoorController, "Lỗi khi đóng cửa: ${exception.message}", Toast.LENGTH_SHORT).show()
                }
        }
    }

    private fun logOut(){
        auth.signOut()
        startActivity(Intent(this@DoorController, DangNhap::class.java))
        finish()
    }

}
package com.example.smart_door

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TimePicker
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.firebase.Firebase
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.FirebaseDatabase

class DangKy : AppCompatActivity() {

    private lateinit var btnQuayLai : Button
    private lateinit var btnDangKy2 : Button
    private lateinit var edtMail2 : EditText
    private lateinit var edtPass2 : EditText
    private lateinit var edtIdEsp32 : EditText
    private lateinit var auth: FirebaseAuth

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_dang_ky)

        auth = FirebaseAuth.getInstance()

        btnQuayLai = findViewById(R.id.btnQuayLai)
        btnDangKy2 = findViewById(R.id.btnDangKy2)
        edtMail2 = findViewById(R.id.edtMail2)
        edtPass2 = findViewById(R.id.edtPass2)
        edtIdEsp32  = findViewById(R.id.edtIdEsp32)

        btnQuayLai.setOnClickListener{
            //Chuyen ve trang dang nhap
            startActivity(Intent(this@DangKy, DangNhap::class.java))
        }

        btnDangKy2.setOnClickListener{
            val mail = edtMail2.text.toString()
            val password = edtPass2.text.toString()
            val idEsp32 = edtIdEsp32.text.toString()

            signUp(mail, password, idEsp32)
        }


    }

    private fun signUp(email: String, password: String , idEsp32: String){
        if(email.isEmpty() || password.isEmpty() || idEsp32.isEmpty()){
            Toast.makeText(this@DangKy, "Thiếu thông tin đăng ký", Toast.LENGTH_SHORT).show()
            return
        }

        auth.createUserWithEmailAndPassword(email, password)
            .addOnCompleteListener(this@DangKy){ task ->
                if(task.isSuccessful){
                    val user = auth.currentUser

                    if(user != null){
                        checkIdEsp32(idEsp32);
                    }
                }
                else{
                    Toast.makeText(this@DangKy, "Lỗi đăng ký", Toast.LENGTH_SHORT).show()
                }
            }
    }

    private fun checkIdEsp32(idEsp32: String) {
        val database = FirebaseDatabase.getInstance().reference

        database.child("doors").child(idEsp32).get()
            .addOnCompleteListener{ task ->
                if(task.isSuccessful){
                    val snapshot = task.result

                    if(snapshot.exists()){
                        //Neu ton tai
                        val user = auth.currentUser

                        if(user != null){
                            val uid = user.uid
                            val userData = mapOf(
                                "idEsp32" to idEsp32
                            )

                            database.child("users").child(uid).setValue(userData)
                                .addOnCompleteListener{ userTask ->
                                    if(userTask.isSuccessful){
                                        Toast.makeText(this@DangKy, "Đăng ký thành công", Toast.LENGTH_SHORT).show()
                                        startActivity(Intent(this@DangKy, DangNhap::class.java))
                                        finish()
                                    }
                                    else{
                                        Toast.makeText(this@DangKy, "Lỗi lưu thông tin", Toast.LENGTH_SHORT).show()
                                        user.delete()
                                    }
                                }

                        }
                    }
                    else{

                        val user = auth.currentUser
                        if(user != null){
                            user.delete()
                                .addOnCompleteListener{deleteTask ->
                                    if(deleteTask.isSuccessful){
                                        Toast.makeText(this@DangKy, "Không tồn tại thiết bị này", Toast.LENGTH_SHORT).show()
                                    }
                                }
                        }
                        else{
                            Toast.makeText(this@DangKy, "Không tồn tại thiết bị này", Toast.LENGTH_SHORT).show()
                        }
                    }
                }
                else{
                    Toast.makeText(this@DangKy, "Lỗi kết nối Database", Toast.LENGTH_SHORT).show()

                    val user = auth.currentUser
                    user?.delete()
                }
            }

    }
}
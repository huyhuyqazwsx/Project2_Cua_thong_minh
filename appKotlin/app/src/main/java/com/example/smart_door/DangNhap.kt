package com.example.smart_door

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.FirebaseDatabase

class DangNhap : AppCompatActivity() {

    private lateinit var btnDangNhap : Button
    private lateinit var btnDangKy : Button
    private lateinit var edtMail : EditText
    private lateinit var edtPass :EditText
    private lateinit var auth: FirebaseAuth

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_dang_nhap)

        auth = FirebaseAuth.getInstance()

        btnDangNhap = findViewById(R.id.btnDangNhap)
        btnDangKy = findViewById(R.id.btnDangKy)
        edtMail = findViewById(R.id.edtMail)
        edtPass = findViewById(R.id.edtPass)

        btnDangNhap.setOnClickListener{
            val email = edtMail.text.toString()
            val password = edtPass.text.toString()
            signIn(email, password)
        }

        btnDangKy.setOnClickListener{
            //Chuyen canh dang ky
            startActivity(Intent(this@DangNhap, DangKy::class.java))
        }


    }

    private fun signIn(email:String,password:String){
        if(email.isEmpty() || password.isEmpty()){
            Toast.makeText(this@DangNhap, "Nhập email và mật khẩu", Toast.LENGTH_SHORT).show()
            return
        }

        auth.signInWithEmailAndPassword(email, password)
            .addOnCompleteListener(this@DangNhap){ task ->
                if(task.isSuccessful){
                    Toast.makeText(this@DangNhap, "Đăng nhập thành công", Toast.LENGTH_SHORT).show()
                    //Chuyen man hinh
                    startActivity(Intent(this@DangNhap, DoorController::class.java))
                    finish()
                }
                else{
                    Toast.makeText(this@DangNhap, "Lỗi đăng nhập: ${task.exception?.message}", Toast.LENGTH_SHORT).show()
                }
            }

    }
}
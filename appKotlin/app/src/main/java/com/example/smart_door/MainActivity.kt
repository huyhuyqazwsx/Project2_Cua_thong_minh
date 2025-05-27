package com.example.smart_door

import android.content.Intent
import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.firebase.auth.FirebaseAuth

class MainActivity : AppCompatActivity() {

    private lateinit var auth : FirebaseAuth

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)

        auth = FirebaseAuth.getInstance()

        val currentUser = auth.currentUser

        //Kiem tra dang nhap
        if(currentUser == null){
            //Neu chua dang nhap
            startActivity(Intent(this@MainActivity, DangNhap::class.java))
        }
        else{
            //Neu da dang nhap
            startActivity(Intent(this@MainActivity, DoorController::class.java))
        }

        finish()

    }
}
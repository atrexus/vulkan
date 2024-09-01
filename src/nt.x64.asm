.code

EXTERN NtGetSSN: PROC

NtOpenProcess PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 0F052CFE3h         ; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtOpenProcess ENDP

NtClose PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 092D1Bfh			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtClose ENDP

NtAllocateVirtualMemory PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 0b52fcbd1h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtAllocateVirtualMemory ENDP

NtWriteVirtualMemory PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 0b719bd8bh			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtWriteVirtualMemory ENDP

NtQueryInformationProcess PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 619e7c0eh			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtQueryInformationProcess ENDP

NtQueryObject PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 684606abh			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtQueryObject ENDP

NtSetIoCompletion PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 5cda7c49h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtSetIoCompletion ENDP

NtQuerySystemInformation PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 9b359fa6h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtQuerySystemInformation ENDP

NtReadVirtualMemory PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 1911911h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtReadVirtualMemory ENDP

NtSuspendProcess PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 0d12acea0h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtSuspendProcess ENDP

NtResumeProcess PROC
	mov [rsp +8], rcx           ; Save registers.
	mov [rsp+16], rdx
	mov [rsp+24], r8
	mov [rsp+32], r9
	sub rsp, 28h
	mov ecx, 9e3b99a3h			; Load function hash into ECX.
	call NtGetSSN				; Resolve function hash into syscall number.
	add rsp, 28h
	mov rcx, [rsp+8]            ; Restore registers.
	mov rdx, [rsp+16]
	mov r8, [rsp+24]
	mov r9, [rsp+32]
	mov r10, rcx
	syscall                     ; Invoke system call.
	ret
NtResumeProcess ENDP

END